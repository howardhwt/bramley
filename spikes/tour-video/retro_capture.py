# Records a libretro core's video frame by frame, with scripted button presses, for the launch film's
# gameplay shot. A minimal frontend through ctypes: no window, no audio, deterministic. Not shipped.
#   python retro_capture.py core.so game.gb out-dir frames "frame:button,frame:button-up,..."
# Buttons: a b start select up down left right. Writes out-dir/00000.png ...
import ctypes as C, sys, os
from PIL import Image

core_path, rom_path, out, n_frames = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4])
script = sys.argv[5] if len(sys.argv) > 5 else ""
os.makedirs(out, exist_ok=True)
lib = C.CDLL(core_path)

IDS = {"b": 0, "y": 1, "select": 2, "start": 3, "up": 4, "down": 5, "left": 6, "right": 7, "a": 8, "x": 9}
held = set()
events = {}
for item in filter(None, script.split(",")):
    f, b = item.split(":")
    events.setdefault(int(f), []).append(b)

ENV = C.CFUNCTYPE(C.c_bool, C.c_uint, C.c_void_p)
VIDEO = C.CFUNCTYPE(None, C.c_void_p, C.c_uint, C.c_uint, C.c_size_t)
AUDIO = C.CFUNCTYPE(None, C.c_int16, C.c_int16)
AUDIOB = C.CFUNCTYPE(C.c_size_t, C.c_void_p, C.c_size_t)
POLL = C.CFUNCTYPE(None)
STATE = C.CFUNCTYPE(C.c_int16, C.c_uint, C.c_uint, C.c_uint, C.c_uint)

fmt = [1]   # RETRO_PIXEL_FORMAT: 0 0RGB1555, 1 XRGB8888, 2 RGB565
sysdir = C.c_char_p(os.path.abspath(out).encode())
last = [None]

def env(cmd, data):
    if cmd == 10:   # SET_PIXEL_FORMAT
        fmt[0] = C.cast(data, C.POINTER(C.c_int)).contents.value; return True
    if cmd in (9, 31):   # GET_SYSTEM_DIRECTORY, GET_SAVE_DIRECTORY
        C.cast(data, C.POINTER(C.c_char_p))[0] = sysdir.value; return True
    if cmd == 3:   # GET_CAN_DUPE
        C.cast(data, C.POINTER(C.c_bool))[0] = True; return True
    return False

def video(data, w, h, pitch):
    if not data: return
    buf = C.string_at(data, pitch * h)
    if fmt[0] == 2:
        im = Image.frombuffer("RGB", (w, h), buf, "raw", "BGR;16", pitch, 1)
    else:
        im = Image.frombuffer("RGBX", (w, h), buf, "raw", "BGRX", pitch, 1).convert("RGB")
    last[0] = im.copy()

def state(port, device, index, idn):
    return 1 if port == 0 and device == 1 and any(IDS[b] == idn for b in held) else 0

cb = [ENV(env), VIDEO(video), AUDIO(lambda l, r: None), AUDIOB(lambda d, n: n), POLL(lambda: None), STATE(state)]
lib.retro_set_environment(cb[0]); lib.retro_set_video_refresh(cb[1]); lib.retro_set_audio_sample(cb[2])
lib.retro_set_audio_sample_batch(cb[3]); lib.retro_set_input_poll(cb[4]); lib.retro_set_input_state(cb[5])
lib.retro_init()

class GameInfo(C.Structure):
    _fields_ = [("path", C.c_char_p), ("data", C.c_void_p), ("size", C.c_size_t), ("meta", C.c_char_p)]
rom = open(rom_path, "rb").read()
rb = C.create_string_buffer(rom, len(rom))
gi = GameInfo(os.path.abspath(rom_path).encode(), C.cast(rb, C.c_void_p), len(rom), None)
lib.retro_load_game.restype = C.c_bool
assert lib.retro_load_game(C.byref(gi)), "load failed"

for f in range(n_frames):
    for b in events.get(f, []):
        if b.endswith("-up"): held.discard(b[:-3])
        else: held.add(b)
    lib.retro_run()
    if last[0] is not None:
        last[0].save(os.path.join(out, f"{f:05d}.png"))
print("frames", n_frames, "format", fmt[0], "size", last[0].size if last[0] else None)
