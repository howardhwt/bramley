// Renders the real notification.c HUD and toasts over a game frame, for checking on the Mac.
#include "../../../workspace/all/common/notification.c"
#include <SDL2/SDL_image.h>
int main(int argc, char** argv) {
	InitSettings();
	TTF_Init();
	screen_width = 1024, screen_height = 768;
	const char* out = argv[1];
	SDL_Surface* game = IMG_Load("/tmp/bhl/ingame/game0-big.png");
	for (int k = 0; k < 2; k++) {
		gl_notification_surface = SDL_CreateRGBSurfaceWithFormat(0, 1024, 768, 32, SDL_PIXELFORMAT_ABGR8888);
		SDL_FillRect(gl_notification_surface, NULL, 0);
		notification_count = 0;
		if (k == 0) {
			system_indicator_type = SYSTEM_INDICATOR_VOLUME;
			render_system_indicator();
		} else {
			notification_count = 2;
			notifications[0] = (Notification){ NOTIFICATION_ACHIEVEMENT, "Achievement unlocked: First Steps" };
			notifications[1] = (Notification){ NOTIFICATION_SAVE_STATE, "Saved to slot 3" };
			render_notification_stack();
		}
		SDL_Surface* frame = SDL_CreateRGBSurfaceWithFormat(0, 1024, 768, 32, SDL_PIXELFORMAT_ABGR8888);
		SDL_FillRect(frame, NULL, 0xff000000);
		SDL_BlitScaled(game, NULL, frame, &(SDL_Rect){0, 42, 1024, 683});
		SDL_SetSurfaceBlendMode(gl_notification_surface, SDL_BLENDMODE_BLEND);
		SDL_BlitSurface(gl_notification_surface, NULL, frame, NULL);
		char p[512];
		snprintf(p, sizeof(p), "%s-%d.bmp", out, k);
		SDL_SaveBMP(frame, p);
	}
	return 0;
}
