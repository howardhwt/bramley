#include "notification.h"
#include "defines.h"
#include "api.h"
#include "config.h"
#include "msettings.h" // Bramley: the HUD reads the levels itself
#include <string.h>
#include <stdio.h>

///////////////////////////////
// Layout constants (unscaled)
///////////////////////////////

#define NOTIF_PADDING_X      8   // Horizontal padding inside pill
#define NOTIF_PADDING_Y      4   // Vertical padding inside pill
#define NOTIF_MARGIN        12   // Margin from screen edge
#define NOTIF_STACK_GAP      6   // Gap between stacked notifications
#define NOTIF_ICON_GAP       4   // Gap between icon and text

// System indicator sizing (must match GFX_blitHardwareIndicator dimensions)
#define SYS_INDICATOR_EXTRA_PAD  4   // Extra padding for indicator pill

///////////////////////////////
// Internal state
///////////////////////////////

static Notification notifications[NOTIFICATION_MAX_QUEUE];
static int notification_count = 0;
static int initialized = 0;

// Persistent surface for GL rendering
static SDL_Surface* gl_notification_surface = NULL;
static int needs_clear_frame = 0;

// Screen dimensions for layer rendering
static int screen_width = 0;
static int screen_height = 0;

// Visual constants (will be set after init with proper scaling)
static int notif_padding_x;
static int notif_padding_y;
static int notif_margin;
static int notif_stack_gap;
static int notif_icon_gap;

// Track if we need to re-render (only when notifications change)
static int render_dirty = 1;
static int last_notification_count = 0;

///////////////////////////////
// System indicator state
///////////////////////////////

static SystemIndicatorType system_indicator_type = SYSTEM_INDICATOR_NONE;
static uint32_t system_indicator_start_time = 0;
static int system_indicator_dirty = 0;
static int last_system_indicator_type = SYSTEM_INDICATOR_NONE;

///////////////////////////////
// Progress indicator state
//
// Thread safety: progress_state is written by background threads
// (sync engine in ra_integration.c, badge download callbacks in
// ra_badges.c) and read by the main thread during rendering.
// All access is protected by progress_mutex.
///////////////////////////////

#define PROGRESS_TITLE_MAX 48
#define PROGRESS_STRING_MAX 16

typedef struct {
    char title[PROGRESS_TITLE_MAX];
    char progress[PROGRESS_STRING_MAX];
    SDL_Surface* icon;
    uint32_t start_time;
    int active;
    int dirty;
    int persistent;
} ProgressIndicatorState;

static ProgressIndicatorState progress_state = {0};
static SDL_mutex* progress_mutex = NULL;

///////////////////////////////
// Challenge indicator state
//
// Active "challenge" achievements (tracked by id so SHOW/HIDE are idempotent).
// Written from the rcheevos event handler (main thread) and read during
// rendering; guarded by challenge_mutex to match the progress-indicator
// pattern and stay safe regardless of which thread renders.
///////////////////////////////

#define CHALLENGE_MAX_ACTIVE 16

static uint32_t challenge_ids[CHALLENGE_MAX_ACTIVE];
static int challenge_count = 0;
static int challenge_dirty = 0;
static int last_challenge_count = 0;
static SDL_mutex* challenge_mutex = NULL;

///////////////////////////////
// Rounded rectangle drawing
///////////////////////////////

// Draw a filled rounded rectangle directly to RGBA pixel buffer.
// This is separate from GFX_blitPill* functions in api.c because:
// 1. Notifications render to an RGBA surface for GL overlay compositing
// 2. GFX_blitPill* use pre-made theme assets requiring screen format surfaces
// 3. Direct pixel manipulation avoids format conversion overhead during animation
static void draw_rounded_rect(SDL_Surface* surface, int x, int y, int w, int h, int radius, Uint32 color) {
    if (!surface || w <= 0 || h <= 0) return;
    
    // Clamp radius to half the smallest dimension
    if (radius > w / 2) radius = w / 2;
    if (radius > h / 2) radius = h / 2;
    
    Uint32* pixels = (Uint32*)surface->pixels;
    int pitch = surface->pitch / 4; // pitch in pixels (32-bit)
    
    for (int py = 0; py < h; py++) {
        for (int px = 0; px < w; px++) {
            int draw = 1;
            
            // Check corners
            if (px < radius && py < radius) {
                // Top-left corner
                int dx = radius - px - 1;
                int dy = radius - py - 1;
                if (dx * dx + dy * dy > radius * radius) draw = 0;
            } else if (px >= w - radius && py < radius) {
                // Top-right corner
                int dx = px - (w - radius);
                int dy = radius - py - 1;
                if (dx * dx + dy * dy > radius * radius) draw = 0;
            } else if (px < radius && py >= h - radius) {
                // Bottom-left corner
                int dx = radius - px - 1;
                int dy = py - (h - radius);
                if (dx * dx + dy * dy > radius * radius) draw = 0;
            } else if (px >= w - radius && py >= h - radius) {
                // Bottom-right corner
                int dx = px - (w - radius);
                int dy = py - (h - radius);
                if (dx * dx + dy * dy > radius * radius) draw = 0;
            }
            
            if (draw) {
                pixels[(y + py) * pitch + (x + px)] = color;
            }
        }
    }
}

///////////////////////////////
// Internal helpers
///////////////////////////////

static void remove_notification(int index) {
    if (index < 0 || index >= notification_count) return;
    
    // Shift remaining notifications down
    for (int i = index; i < notification_count - 1; i++) {
        notifications[i] = notifications[i + 1];
    }
    notification_count--;
    render_dirty = 1;
}

///////////////////////////////
// Public API
///////////////////////////////

void Notification_init(void) {
    notification_count = 0;
    memset(notifications, 0, sizeof(notifications));
    
    // Initialize scaled visual constants (compact pills)
    notif_padding_x = SCALE1(NOTIF_PADDING_X);
    notif_padding_y = SCALE1(NOTIF_PADDING_Y);
    notif_margin = SCALE1(NOTIF_MARGIN);
    notif_stack_gap = SCALE1(NOTIF_STACK_GAP);
    notif_icon_gap = SCALE1(NOTIF_ICON_GAP);
    
    // Store screen dimensions for layer rendering
    screen_width = FIXED_WIDTH;
    screen_height = FIXED_HEIGHT;
    
    // Create progress indicator mutex (background threads write progress_state)
    if (!progress_mutex) {
        progress_mutex = SDL_CreateMutex();
    }
    // Create challenge indicator mutex
    if (!challenge_mutex) {
        challenge_mutex = SDL_CreateMutex();
    }
    challenge_count = 0;
    challenge_dirty = 0;
    last_challenge_count = 0;

    render_dirty = 1;
    last_notification_count = 0;
    initialized = 1;
}

void Notification_push(NotificationType type, const char* message, SDL_Surface* icon) {
    if (!initialized) {
        return;
    }
    
    // Check if notifications are enabled for this type
    if ((type == NOTIFICATION_ACHIEVEMENT || type == NOTIFICATION_OFFLINE_ACHIEVEMENT) && !CFG_getRAShowNotifications()) {
        return;
    }
    
    // If queue is full, remove oldest notification
    if (notification_count >= NOTIFICATION_MAX_QUEUE) {
        remove_notification(0);
    }
    
    // Add new notification at end of queue
    Notification* n = &notifications[notification_count];
    n->type = type;
    strncpy(n->message, message, NOTIFICATION_MAX_MESSAGE - 1);
    n->message[NOTIFICATION_MAX_MESSAGE - 1] = '\0';
    n->icon = icon;
    n->start_time = SDL_GetTicks();
    
    // Use RA-specific duration for achievement notifications
    if (type == NOTIFICATION_ACHIEVEMENT || type == NOTIFICATION_OFFLINE_ACHIEVEMENT) {
        n->duration_ms = CFG_getRANotificationDuration() * 1000;
    } else {
        n->duration_ms = CFG_getNotifyDuration() * 1000;
    }
    n->state = NOTIFICATION_STATE_VISIBLE;
    
    notification_count++;
    render_dirty = 1;
}

void Notification_update(uint32_t now) {
    if (!initialized) return;
    
    // Update system indicator timeout
    if (system_indicator_type != SYSTEM_INDICATOR_NONE) {
        uint32_t elapsed = now - system_indicator_start_time;
        if (elapsed >= SYSTEM_INDICATOR_DURATION_MS) {
            system_indicator_type = SYSTEM_INDICATOR_NONE;
            system_indicator_dirty = 1;
        }
    }
    
	// Update progress indicator timeout (skip if persistent).
	// Read-check-write must be atomic to avoid a TOCTOU race: a background
	// thread can call showProgressIndicator() between our read and write,
	// resetting start_time.  If we then write active=0 based on the stale
	// snapshot, the new notification is silently killed.
	SDL_LockMutex(progress_mutex);
	if (progress_state.active && !progress_state.persistent) {
		uint32_t elapsed = now - progress_state.start_time;
		int duration_seconds = CFG_getRAProgressNotificationDuration();
		if (duration_seconds > 0 && elapsed >= (uint32_t)(duration_seconds * 1000)) {
			progress_state.active = 0;
			progress_state.dirty = 1;
		}
	}
	SDL_UnlockMutex(progress_mutex);
    
    // Check each notification for expiration
    for (int i = 0; i < notification_count; i++) {
        Notification* n = &notifications[i];
        uint32_t elapsed = now - n->start_time;
        
        if (n->state == NOTIFICATION_STATE_VISIBLE && elapsed >= n->duration_ms) {
            n->state = NOTIFICATION_STATE_DONE;
        }
    }
    
    // Remove completed notifications (iterate backwards to avoid index issues)
    for (int i = notification_count - 1; i >= 0; i--) {
        if (notifications[i].state == NOTIFICATION_STATE_DONE) {
            remove_notification(i);
        }
    }
}

// Render system indicator (top-right)
// Width formula must match GFX_blitHardwareIndicator in api.c:
//   SCALE1(PILL_SIZE + SETTINGS_WIDTH + 10 + 4)
// The 10 is internal pill content padding (not the screen-edge PADDING macro).
///////////////////////////////
// Bramley: the volume and brightness HUD and the toasts, drawn in Bramley's style: dark glass
// pills with Inter text. The HUD sits at the top centre; toasts at the bottom centre.
///////////////////////////////

static TTF_Font* bram_font;   // Inter, for toasts
static TTF_Font* bram_small;  // Inter SemiBold, for the HUD's percent

static void bram_fonts(void) {
    if (!bram_font) bram_font = TTF_OpenFont(RES_PATH "/Inter-Regular.ttf", 22);
    if (!bram_small) bram_small = TTF_OpenFont(RES_PATH "/Inter-SemiBold.ttf", 18);
}

// Blends one RGBA pixel over the overlay (straight alpha).
static void bram_blend(SDL_Surface* s, int x, int y, float r, float g, float b, float a) {
    if (x < 0 || y < 0 || x >= s->w || y >= s->h || a <= 0) return;
    Uint8* p = (Uint8*)s->pixels + y * s->pitch + x * 4; // ABGR8888: R, G, B, A in memory
    float da = p[3] / 255.0f, oa = a + da * (1 - a);
    if (oa <= 0) return;
    p[0] = (Uint8)((r * a + p[0] / 255.0f * da * (1 - a)) / oa * 255);
    p[1] = (Uint8)((g * a + p[1] / 255.0f * da * (1 - a)) / oa * 255);
    p[2] = (Uint8)((b * a + p[2] / 255.0f * da * (1 - a)) / oa * 255);
    p[3] = (Uint8)(oa * 255);
}

// A filled rounded rectangle with smooth edges: coverage from the signed distance to the shape.
static void bram_round(SDL_Surface* s, float x, float y, float w, float h, float rad, float r, float g, float b, float a) {
    if (rad > w / 2) rad = w / 2;
    if (rad > h / 2) rad = h / 2;
    float hx = w * 0.5f - rad, hy = h * 0.5f - rad, mx = x + w * 0.5f, my = y + h * 0.5f;
    for (int py = (int)y - 1; py <= (int)(y + h) + 1; py++)
        for (int px = (int)x - 1; px <= (int)(x + w) + 1; px++) {
            float qx = SDL_fabsf(px + 0.5f - mx) - hx, qy = SDL_fabsf(py + 0.5f - my) - hy;
            float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
            float inside = qx > qy ? qx : qy;
            float d = SDL_sqrtf(ox * ox + oy * oy) + (inside < 0 ? inside : 0) - rad;
            float cover = 0.5f - d;
            if (cover > 1) cover = 1;
            if (cover > 0) bram_blend(s, px, py, r, g, b, a * cover);
        }
}

static void bram_glass(SDL_Surface* s, float x, float y, float w, float h) {
    bram_round(s, x - 2, y + 3, w + 4, h + 4, h * 0.5f + 2, 0, 0, 0, 0.18f); // soft shadow
    bram_round(s, x, y, w, h, h * 0.5f, 0.11f, 0.11f, 0.13f, 0.88f);
}

static void bram_text(SDL_Surface* s, TTF_Font* f, const char* t, int x, int y, Uint8 alpha) {
    if (!f || !t || !*t) return;
    SDL_Surface* ts = TTF_RenderUTF8_Blended(f, t, (SDL_Color){255, 255, 255, 255});
    if (!ts) return;
    SDL_SetSurfaceAlphaMod(ts, alpha);
    SDL_SetSurfaceBlendMode(ts, SDL_BLENDMODE_BLEND);
    SDL_BlitSurface(ts, NULL, s, &(SDL_Rect){x, y, ts->w, ts->h});
    SDL_FreeSurface(ts);
}

// Speaker: a body and a cone, then a wave for each third of the volume.
static void bram_speaker(SDL_Surface* s, float x, float y, int level, int max) {
    bram_round(s, x, y + 8, 7, 10, 1.5f, 1, 1, 1, 1);
    for (int i = 0; i < 7; i++) bram_round(s, x + 6 + i, y + 8 - i * 1.1f, 1.6f, 10 + i * 2.2f, 0.5f, 1, 1, 1, 1);
    int waves = level <= 0 ? 0 : 1 + (level * 2) / (max > 0 ? max : 1);
    for (int w = 0; w < waves && w < 3; w++) {
        float r = 6 + w * 5, cx = x + 13, cy = y + 13;
        for (float a = -0.9f; a <= 0.9f; a += 0.02f)
            bram_round(s, cx + r * SDL_cosf(a) - 1, cy + r * SDL_sinf(a) - 1, 2.2f, 2.2f, 1.1f, 1, 1, 1, 0.9f);
    }
    if (level <= 0) // muted: a slash
        for (int i = 0; i < 18; i++) bram_round(s, x + 14 + i * 0.7f, y + 4 + i, 2.2f, 2.2f, 1.1f, 1, 1, 1, 1);
}

static void bram_sun(SDL_Surface* s, float cx, float cy) {
    bram_round(s, cx - 6, cy - 6, 12, 12, 6, 1, 1, 1, 1);
    for (int i = 0; i < 8; i++) {
        float a = i * 0.7854f;
        bram_round(s, cx + SDL_cosf(a) * 11 - 2, cy + SDL_sinf(a) * 11 - 2, 4, 4, 2, 1, 1, 1, 1);
    }
}

static void render_system_indicator(void) {
    bram_fonts();
    int value, lo, hi, volume = system_indicator_type == SYSTEM_INDICATOR_VOLUME;
    if (system_indicator_type == SYSTEM_INDICATOR_BRIGHTNESS) value = GetBrightness(), lo = BRIGHTNESS_MIN, hi = BRIGHTNESS_MAX;
    else if (system_indicator_type == SYSTEM_INDICATOR_COLORTEMP) value = GetColortemp(), lo = COLORTEMP_MIN, hi = COLORTEMP_MAX;
    else value = GetVolume(), lo = VOLUME_MIN, hi = VOLUME_MAX;
    float frac = hi > lo ? (float)(value - lo) / (hi - lo) : 0;
    if (frac < 0) frac = 0;
    if (frac > 1) frac = 1;
    float w = 300, h = 52, x = (screen_width - w) * 0.5f, y = 22;
    bram_glass(gl_notification_surface, x, y, w, h);
    if (volume) bram_speaker(gl_notification_surface, x + 18, y + 13, value - lo, hi - lo);
    else bram_sun(gl_notification_surface, x + 32, y + 26);
    float bx = x + 62, bw = w - 62 - 70, by = y + h * 0.5f - 4;
    bram_round(gl_notification_surface, bx, by, bw, 8, 4, 1, 1, 1, 0.2f);
    if (frac > 0) bram_round(gl_notification_surface, bx, by, bw * frac < 8 ? 8 : bw * frac, 8, 4, 1, 1, 1, 1);
    char pct[16];
    snprintf(pct, sizeof(pct), "%d%%", (int)(frac * 100 + 0.5f));
    int tw = 0, th = 0;
    if (bram_small) TTF_SizeUTF8(bram_small, pct, &tw, &th);
    bram_text(gl_notification_surface, bram_small, pct, (int)(x + w - 18 - tw), (int)(y + (h - th) * 0.5f), 220);
}

// Render progress indicator pill (top-left)
// Takes a snapshot of progress state to avoid holding the mutex during rendering.
static void render_progress_indicator(const ProgressIndicatorState* snap) {
	SDL_Color text_color = uintToColour(THEME_COLOR1_255);
	SDL_Color bg_color_sdl = uintToColour(THEME_COLOR2_255);
	
	// Format: "Title: Progress" or just "Title"
	char progress_text[PROGRESS_TITLE_MAX + PROGRESS_STRING_MAX + 4];
	if (snap->progress[0] != '\0') {
		snprintf(progress_text, sizeof(progress_text), "%s: %s", 
		         snap->title, snap->progress);
	} else {
		snprintf(progress_text, sizeof(progress_text), "%s", snap->title);
	}
	
	int text_w = 0, text_h = 0;
	TTF_SizeUTF8(font.tiny, progress_text, &text_w, &text_h);
	
	// Calculate icon dimensions if present
	int icon_w = 0, icon_h = 0, icon_total_w = 0;
	if (snap->icon) {
		icon_h = text_h;
		icon_w = (snap->icon->w * icon_h) / snap->icon->h;
		icon_total_w = icon_w + notif_icon_gap;
	}
	
	int pill_w = icon_total_w + text_w + (notif_padding_x * 2);
	int pill_h = text_h + (notif_padding_y * 2);
	int corner_radius = pill_h / 2;
	int x = notif_margin;
	int y = notif_margin;
	
	SDL_Surface* progress_surface = SDL_CreateRGBSurfaceWithFormat(
		0, pill_w, pill_h, 32, SDL_PIXELFORMAT_ABGR8888
	);
	if (!progress_surface) return;
	
	SDL_FillRect(progress_surface, NULL, 0);
	Uint32 bg_color = SDL_MapRGBA(progress_surface->format, 
	                               bg_color_sdl.r, bg_color_sdl.g, bg_color_sdl.b, 255);
	draw_rounded_rect(progress_surface, 0, 0, pill_w, pill_h, corner_radius, bg_color);
	
	int content_x = notif_padding_x;
	
	if (snap->icon && icon_w > 0 && icon_h > 0) {
		SDL_Rect icon_dst = {content_x, notif_padding_y, icon_w, icon_h};
		SDL_SetSurfaceBlendMode(snap->icon, SDL_BLENDMODE_BLEND);
		SDL_BlitScaled(snap->icon, NULL, progress_surface, &icon_dst);
		content_x += icon_total_w;
	}
	
	SDL_Surface* text_surf = TTF_RenderUTF8_Blended(font.tiny, progress_text, text_color);
	if (text_surf) {
		SDL_SetSurfaceBlendMode(text_surf, SDL_BLENDMODE_BLEND);
		SDL_Rect text_dst = {content_x, notif_padding_y, text_surf->w, text_surf->h};
		SDL_BlitSurface(text_surf, NULL, progress_surface, &text_dst);
		SDL_FreeSurface(text_surf);
	}
	
	SDL_SetSurfaceBlendMode(progress_surface, SDL_BLENDMODE_NONE);
	SDL_Rect dst_rect = {x, y, pill_w, pill_h};
	SDL_BlitSurface(progress_surface, NULL, gl_notification_surface, &dst_rect);
	SDL_FreeSurface(progress_surface);
}

// Render the challenge indicator: a trophy pill in the lower-right corner.
// One trophy when a single challenge is active; trophy + "+N" when more than
// one. `count` is the number of active challenges (>= 1).
static void render_challenge_indicator(int count) {
	if (count < 1) return;

	SDL_Color text_color = uintToColour(THEME_COLOR1_255);
	SDL_Color bg_color_sdl = uintToColour(THEME_COLOR2_255);

	// Trophy glyph size (the asset rect is already scaled to the device scale).
	SDL_Rect trophy_rect;
	GFX_assetRect(ASSET_TROPHY, &trophy_rect);
	int icon_w = trophy_rect.w;
	int icon_h = trophy_rect.h;

	// "+N" counter only when more than one challenge is active.
	char count_text[8] = {0};
	int text_w = 0, text_h = 0;
	if (count > 1) {
		snprintf(count_text, sizeof(count_text), "+%d", count - 1);
		TTF_SizeUTF8(font.tiny, count_text, &text_w, &text_h);
	}

	int content_h = icon_h > text_h ? icon_h : text_h;
	int gap = (count > 1) ? notif_icon_gap : 0;
	int pill_w = icon_w + gap + text_w + (notif_padding_x * 2);
	int pill_h = content_h + (notif_padding_y * 2);
	int corner_radius = pill_h / 2;
	int x = screen_width - notif_margin - pill_w;
	int y = screen_height - notif_margin - pill_h;

	SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
		0, pill_w, pill_h, 32, SDL_PIXELFORMAT_ABGR8888);
	if (!surface) return;

	SDL_FillRect(surface, NULL, 0);
	Uint32 bg_color = SDL_MapRGBA(surface->format,
	                              bg_color_sdl.r, bg_color_sdl.g, bg_color_sdl.b, 255);
	draw_rounded_rect(surface, 0, 0, pill_w, pill_h, corner_radius, bg_color);

	int content_x = notif_padding_x;
	int icon_y = notif_padding_y + (content_h - icon_h) / 2;
	GFX_blitAssetColor(ASSET_TROPHY, NULL, surface,
	                   &(SDL_Rect){content_x, icon_y}, THEME_COLOR1_255);
	content_x += icon_w + gap;

	if (count > 1) {
		SDL_Surface* text_surf = TTF_RenderUTF8_Blended(font.tiny, count_text, text_color);
		if (text_surf) {
			SDL_SetSurfaceBlendMode(text_surf, SDL_BLENDMODE_BLEND);
			SDL_Rect text_dst = {content_x,
			                     notif_padding_y + (content_h - text_surf->h) / 2,
			                     text_surf->w, text_surf->h};
			SDL_BlitSurface(text_surf, NULL, surface, &text_dst);
			SDL_FreeSurface(text_surf);
		}
	}

	SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_NONE);
	SDL_Rect dst_rect = {x, y, pill_w, pill_h};
	SDL_BlitSurface(surface, NULL, gl_notification_surface, &dst_rect);
	SDL_FreeSurface(surface);
}

// Render a single notification pill
// Bramley: toasts at the bottom centre, newest at the bottom, each with a coloured dot (green for
// saves, gold for achievements, blue for the rest) or the achievement's badge.
static void render_notification_stack(void) {
    bram_fonts();
    if (!bram_font) return;
    float h = 52, gap = 10, y = screen_height - 88 - h; // clear of the letterbox and the edge
    for (int i = notification_count - 1; i >= 0; i--, y -= h + gap) {
        Notification* n = &notifications[i];
        int tw = 0, th = 0;
        TTF_SizeUTF8(bram_font, n->message, &tw, &th);
        float w = tw + 76, x = (screen_width - w) * 0.5f;
        bram_glass(gl_notification_surface, x, y, w, h);
        if (n->icon) {
            SDL_Rect dst = {(int)x + 14, (int)y + 10, 32, 32};
            SDL_SetSurfaceBlendMode(n->icon, SDL_BLENDMODE_BLEND);
            SDL_BlitScaled(n->icon, NULL, gl_notification_surface, &dst);
        } else {
            int ach = n->type == NOTIFICATION_ACHIEVEMENT || n->type == NOTIFICATION_OFFLINE_ACHIEVEMENT;
            int save = n->type == NOTIFICATION_SAVE_STATE || n->type == NOTIFICATION_LOAD_STATE;
            float r = save ? 0.20f : ach ? 1.0f : 0.04f, g = save ? 0.78f : ach ? 0.75f : 0.52f, b = save ? 0.35f : ach ? 0.2f : 1.0f;
            bram_round(gl_notification_surface, x + 20, y + 16, 20, 20, ach ? 5 : 10, r, g, b, 1);
        }
        bram_text(gl_notification_surface, bram_font, n->message, (int)(x + 54), (int)(y + (h - th) * 0.5f), 255);
    }
}

void Notification_renderToLayer(int layer) {
    (void)layer; // unused now, kept for API compatibility
    
    if (!initialized) {
        PLAT_clearNotificationSurface();
        return;
    }
    
	int has_notifications = notification_count > 0;
	int has_system_indicator = system_indicator_type != SYSTEM_INDICATOR_NONE;
	
	// Snapshot progress state under lock — sub-microsecond critical section.
	// Rendering uses the snapshot so we never hold the lock during SDL calls.
	ProgressIndicatorState progress_snap;
	SDL_LockMutex(progress_mutex);
	progress_snap = progress_state;
	SDL_UnlockMutex(progress_mutex);
	
	int has_progress_indicator = progress_snap.active;

	// Snapshot challenge indicator state under lock.
	int challenge_snap = 0, challenge_snap_dirty = 0;
	SDL_LockMutex(challenge_mutex);
	challenge_snap = challenge_count;
	challenge_snap_dirty = challenge_dirty;
	SDL_UnlockMutex(challenge_mutex);
	int has_challenge_indicator = challenge_snap > 0;

	if (!has_notifications && !has_system_indicator && !has_progress_indicator && !has_challenge_indicator) {
		// When all notifications and indicators are gone, render one final transparent frame
		if (gl_notification_surface) {
			if (needs_clear_frame) {
				SDL_FillRect(gl_notification_surface, NULL, 0);
				PLAT_setNotificationSurface(gl_notification_surface, 0, 0);
				needs_clear_frame = 0;
				render_dirty = 0;
				system_indicator_dirty = 0;
				SDL_LockMutex(progress_mutex);
				progress_state.dirty = 0;
				SDL_UnlockMutex(progress_mutex);
				SDL_LockMutex(challenge_mutex);
				challenge_dirty = 0;
				SDL_UnlockMutex(challenge_mutex);
				last_challenge_count = 0;
				last_system_indicator_type = SYSTEM_INDICATOR_NONE;
				return;
			}
			PLAT_clearNotificationSurface();
			SDL_FreeSurface(gl_notification_surface);
			gl_notification_surface = NULL;
		}
		return;
	}
	
	// We have notifications or indicators
	needs_clear_frame = 1;
	
	// Check if anything changed
	int notifications_changed = render_dirty || notification_count != last_notification_count;
	int indicator_changed = system_indicator_dirty || system_indicator_type != last_system_indicator_type;
	int progress_changed = progress_snap.dirty;
	int challenge_changed = challenge_snap_dirty || challenge_snap != last_challenge_count;

    if (!notifications_changed && !indicator_changed && !progress_changed && !challenge_changed) {
        return;
    }
    
    // Create surface if needed
    if (!gl_notification_surface) {
        gl_notification_surface = SDL_CreateRGBSurfaceWithFormat(
            0, screen_width, screen_height, 32, SDL_PIXELFORMAT_ABGR8888
        );
        if (!gl_notification_surface) {
            return;
        }
    }
    
    // Clear to transparent
    SDL_FillRect(gl_notification_surface, NULL, 0);
    
    // Render each element type
    if (has_system_indicator) {
        render_system_indicator();
    }
	if (has_progress_indicator) {
		render_progress_indicator(&progress_snap);
	}
	if (has_challenge_indicator) {
		render_challenge_indicator(challenge_snap);
	}
    if (has_notifications) {
        render_notification_stack();
    }
    
    // Set the notification surface for GL rendering
    PLAT_setNotificationSurface(gl_notification_surface, 0, 0);
    
	render_dirty = 0;
	last_notification_count = notification_count;
	system_indicator_dirty = 0;
	last_system_indicator_type = system_indicator_type;
	
	// Clear dirty flag under lock — a background thread may have set it again
	// since our snapshot, in which case we'll re-render next frame.
	SDL_LockMutex(progress_mutex);
	progress_state.dirty = 0;
	SDL_UnlockMutex(progress_mutex);

	last_challenge_count = challenge_snap;
	SDL_LockMutex(challenge_mutex);
	challenge_dirty = 0;
	SDL_UnlockMutex(challenge_mutex);
}

bool Notification_isActive(void) {
    return initialized && notification_count > 0;
}

void Notification_clear(void) {
	notification_count = 0;
	
	SDL_LockMutex(progress_mutex);
	progress_state.active = 0;
	if (progress_state.icon) {
		SDL_FreeSurface(progress_state.icon);
		progress_state.icon = NULL;
	}
	progress_state.dirty = 1;
	SDL_UnlockMutex(progress_mutex);

	if (challenge_mutex) {
		SDL_LockMutex(challenge_mutex);
		challenge_count = 0;
		challenge_dirty = 1;
		SDL_UnlockMutex(challenge_mutex);
	}

	render_dirty = 1;
	PLAT_clearNotificationSurface();
    if (gl_notification_surface) {
        SDL_FreeSurface(gl_notification_surface);
        gl_notification_surface = NULL;
    }
}

void Notification_quit(void) {
    Notification_clear();
    system_indicator_type = SYSTEM_INDICATOR_NONE;
    initialized = 0;

    if (progress_mutex) {
        SDL_DestroyMutex(progress_mutex);
        progress_mutex = NULL;
    }
    if (challenge_mutex) {
        SDL_DestroyMutex(challenge_mutex);
        challenge_mutex = NULL;
    }
}

///////////////////////////////
// System Indicator Functions
///////////////////////////////

void Notification_showSystemIndicator(SystemIndicatorType type) {
    if (!initialized) return;
    if (type == SYSTEM_INDICATOR_NONE) return;
    
    // Update or start the indicator
    system_indicator_type = type;
    system_indicator_start_time = SDL_GetTicks();
    system_indicator_dirty = 1;
}

bool Notification_hasSystemIndicator(void) {
    return initialized && system_indicator_type != SYSTEM_INDICATOR_NONE;
}

int Notification_getSystemIndicatorWidth(void) {
    if (!initialized || system_indicator_type == SYSTEM_INDICATOR_NONE) {
        return 0;
    }
    // Must match GFX_blitHardwareIndicator width formula (10 = internal pill padding)
    return SCALE1(PILL_SIZE + SETTINGS_WIDTH + 10 + SYS_INDICATOR_EXTRA_PAD);
}

///////////////////////////////
// Progress Indicator Functions
///////////////////////////////

void Notification_showProgressIndicator(const char* title, const char* progress, SDL_Surface* icon) {
	if (!initialized) return;
	
	// Check if RA notifications are enabled
	if (!CFG_getRAShowNotifications()) return;
	
	// Called from background threads (sync engine, badge downloads)
	SDL_LockMutex(progress_mutex);
	
	// Copy the title and progress strings
	strncpy(progress_state.title, title, PROGRESS_TITLE_MAX - 1);
	progress_state.title[PROGRESS_TITLE_MAX - 1] = '\0';
	
	strncpy(progress_state.progress, progress, PROGRESS_STRING_MAX - 1);
	progress_state.progress[PROGRESS_STRING_MAX - 1] = '\0';
	
	// Duplicate the icon so progress_state owns its copy and the badge cache
	// can free its surfaces (e.g. on game unload) without causing a use-after-free.
	SDL_Surface* prev_icon = progress_state.icon;
	progress_state.icon = icon ? SDL_DuplicateSurface(icon) : NULL;
	if (prev_icon) SDL_FreeSurface(prev_icon);
	
	// Activate and reset timer
	progress_state.active = 1;
	progress_state.start_time = SDL_GetTicks();
	progress_state.dirty = 1;
	
	SDL_UnlockMutex(progress_mutex);
}

void Notification_hideProgressIndicator(void) {
	if (!initialized) return;
	
	// Called from background threads (sync engine, badge downloads)
	SDL_LockMutex(progress_mutex);
	if (progress_state.active) {
		progress_state.active = 0;
		progress_state.persistent = 0;
		if (progress_state.icon) {
			SDL_FreeSurface(progress_state.icon);
			progress_state.icon = NULL;
		}
		progress_state.dirty = 1;
	}
	SDL_UnlockMutex(progress_mutex);
}

void Notification_setProgressIndicatorPersistent(bool persistent) {
	// Called from background threads
	SDL_LockMutex(progress_mutex);
	progress_state.persistent = persistent ? 1 : 0;
	SDL_UnlockMutex(progress_mutex);
}

bool Notification_hasProgressIndicator(void) {
    if (!initialized) return false;
    SDL_LockMutex(progress_mutex);
    bool active = progress_state.active;
    SDL_UnlockMutex(progress_mutex);
    return active;
}

///////////////////////////////
// Challenge Indicator Functions
///////////////////////////////

void Notification_showChallengeIndicator(uint32_t ach_id) {
    if (!initialized) return;
    SDL_LockMutex(challenge_mutex);
    int found = 0;
    for (int i = 0; i < challenge_count; i++) {
        if (challenge_ids[i] == ach_id) { found = 1; break; }
    }
    if (!found && challenge_count < CHALLENGE_MAX_ACTIVE) {
        challenge_ids[challenge_count++] = ach_id;
        challenge_dirty = 1;
    }
    SDL_UnlockMutex(challenge_mutex);
}

void Notification_hideChallengeIndicator(uint32_t ach_id) {
    if (!initialized) return;
    SDL_LockMutex(challenge_mutex);
    for (int i = 0; i < challenge_count; i++) {
        if (challenge_ids[i] == ach_id) {
            // Remove by shifting the tail down.
            for (int j = i; j < challenge_count - 1; j++) {
                challenge_ids[j] = challenge_ids[j + 1];
            }
            challenge_count--;
            challenge_dirty = 1;
            break;
        }
    }
    SDL_UnlockMutex(challenge_mutex);
}

void Notification_clearChallengeIndicators(void) {
    if (!challenge_mutex) return;
    SDL_LockMutex(challenge_mutex);
    if (challenge_count > 0) {
        challenge_count = 0;
        challenge_dirty = 1;
    }
    SDL_UnlockMutex(challenge_mutex);
}

bool Notification_hasChallengeIndicator(void) {
    if (!initialized) return false;
    SDL_LockMutex(challenge_mutex);
    bool active = challenge_count > 0;
    SDL_UnlockMutex(challenge_mutex);
    return active;
}
