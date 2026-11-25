#!/usr/bin/env python3
"""Render the LEXXOS prototype boot animation from its approved still image.

Dependencies: numpy, Pillow, OpenCV, ffmpeg.
"""
from pathlib import Path
import numpy as np
import cv2
import subprocess

ROOT = Path(__file__).resolve().parent
SOURCE = ROOT / 'a_clean_high_resolution_dark_sci_fi_graphic_logo.png'
OUT_MP4 = ROOT / 'LexxOS_Black_Ice_Boot_720.mp4'
OUT_MP4_320 = ROOT / 'LexxOS_Black_Ice_Boot_320.mp4'
OUT_GIF = ROOT / 'LexxOS_Black_Ice_Boot_Preview.gif'

SIZE = 720
FPS = 30
DURATION = 7.0
N = int(FPS*DURATION)

im = cv2.imread(str(SOURCE), cv2.IMREAD_COLOR)
if im is None:
    raise RuntimeError(f'Could not read {SOURCE}')
base = cv2.resize(cv2.cvtColor(im, cv2.COLOR_BGR2RGB), (SIZE,SIZE), interpolation=cv2.INTER_AREA).astype(np.float32)
# The selected original has a very dark (not perfectly black) background.
# Remove near-black compression/drawing noise, but keep even faint red pixels.
base = np.clip((base - 3.2) * (255 / (255 - 3.2)), 0, 255)
H,W = base.shape[:2]
Y,X = np.mgrid[0:H,0:W].astype(np.float32)
ratio = SIZE/1254.

# Extract center logotype from the source with softly edged rectangular mask.
left,right = int(307*ratio), int(1014*ratio)
top,bottom = int(556*ratio), int(666*ratio)
font_mask = np.zeros((H,W),np.float32)
font_mask[top:bottom,left:right] = 1.
font_mask = cv2.GaussianBlur(font_mask,(0,0),1.2)
logo = base*font_mask[...,None]
ring = base*(1.-font_mask[...,None])

angle = (np.arctan2(Y-H*0.5,X-W*0.5) + np.pi*0.5)%(np.pi*2)
rad = np.hypot(X-W*.5,Y-H*.5)
# In boot reveal, the glitchy scattered fragments around the core only build
# where the dominant ring is drawing.
rng = np.random.default_rng(1138)
static_noise = rng.random((H,W),dtype=np.float32)
# Discrete offset pattern makes an angular front feel digitally disrupted.
jitter = (np.sin(Y*0.29)*0.028 + np.sin(Y*0.74)*0.011 + (static_noise-0.5)*0.047)

def ease(v):
    a=np.clip(v,0,1)
    return a*a*(3.-2.*a)

def shift_channels(a,red_shift=4,blue_shift=-3):
    out=a.copy()
    out[:,:,0]=np.roll(a[:,:,0],red_shift,axis=1)
    out[:,:,2]=np.roll(a[:,:,2],blue_shift,axis=1)
    return out

def glitch(a,t,strength,seed):
    r=np.random.default_rng(seed)
    out=a.copy()
    strips=int(6 + 20*strength)
    for _ in range(strips):
        y=int(r.integers(10,H-10))
        bh=int(r.integers(1, max(3,int(4+19*strength))))
        offset=int(r.integers(-int(7+45*strength),int(7+45*strength)+1))
        if offset==0: continue
        sl=slice(y,min(y+bh,H))
        out[sl]=np.roll(out[sl],offset,axis=1)
        if r.random()<0.50:
            c=0 if r.random()<.75 else 1
            out[y:min(y+2,H),:,c]=np.maximum(out[y:min(y+2,H),:,c],int(70+140*strength))
    if strength>.25:
        out=shift_channels(out,int(2+6*strength),-int(2+5*strength))
    return out

ffmpeg=[
    'ffmpeg','-y','-loglevel','error', '-f','rawvideo','-vcodec','rawvideo',
    '-pix_fmt','rgb24','-s',f'{W}x{H}', '-r',str(FPS), '-i','-',
    '-an','-c:v','libx264','-preset','medium','-crf','17',
    '-pix_fmt','yuv420p','-movflags','+faststart',str(OUT_MP4)
]
proc=subprocess.Popen(ffmpeg,stdin=subprocess.PIPE,stderr=subprocess.PIPE)
try:
    for f in range(N):
        t=f/FPS
        # 0.00-0.25: startup darkness, with a very brief sync flicker.
        progress=ease((t-0.45)/1.75)
        sweep=progress*2*np.pi
        angular=(angle+jitter)
        # Naturally irregular reveal front, including isolated ahead-of-beam pixels.
        if progress>=1.0:
            ring_alpha=np.ones((H,W),np.float32)
        elif progress<=0:
            ring_alpha=np.zeros((H,W),np.float32)
        else:
            ring_alpha=np.clip((sweep-angular)*18+0.40,0,1).astype(np.float32)
            leading=np.exp(-np.square((angular-sweep)/0.13)) * (static_noise>0.92)
            ring_alpha=np.maximum(ring_alpha,leading.astype(np.float32)*.68)
        # Ring first emerges as a slightly unstable image.
        flicker=(0.84 + .16*np.sin(85*t)**2) if .45<t<2.05 else 1.0
        frame=ring*ring_alpha[...,None]*flicker
        # An understated red scan glows near the ring's expanding right-hand edge.
        if 0.7<t<2.28:
            tip=np.maximum(0,1-np.abs(angular-sweep)/.095)
            band=(rad>220)&(rad<316)
            red=tip*band*18
            frame[:,:,0]+=red
        # The name resolves in horizontal digital bands, left to right.
        letter_phase=ease((t-2.20)/0.78)
        if letter_phase>0:
            sweep_x=left + (right-left)*letter_phase
            # Thin slice variation as though assembling from packets.
            line_variation=np.sin(Y*.26+t*25)*5 + np.sin(Y*.81+t*12)*2
            letter_gate=np.clip((sweep_x - X+line_variation)/4+0.5,0,1)
            # Tiny red/white afterimage is visible during the actual reveal only.
            letter_alpha=letter_gate*(0.87+0.13*ease((t-2.2)/.95))
            frame+=logo*letter_alpha[...,None]
        # One short, intentionally controlled glitch after the composition resolves.
        shock=max(0,1-abs(t-3.36)/.24)
        shock=max(shock, max(0,1-abs(t-3.78)/.12)*.43)
        if shock>.05:
            frame=glitch(frame,t,shock,seed=5000+f)
            frame+=rng.normal(0,3.5*shock,(H,W,1)).astype(np.float32)
            frame=frame*(1+0.18*shock)
        # Boot hand-off: preserve a clean static logo after the effect finishes.
        if t>=4.15:
            frame=base.copy()
        # very brief early jitter on signal acquire
        if .22<t<.29:
            frame+=base * .09
        frame=np.clip(frame,0,255).astype(np.uint8)
        proc.stdin.write(frame.tobytes())
finally:
    proc.stdin.close()
    err=proc.stderr.read().decode('utf-8','replace')
    status=proc.wait()
    if status:
        raise RuntimeError(f'ffmpeg failed: {err}')
print('Wrote',OUT_MP4,OUT_MP4.stat().st_size)

# 320x320 firmware-display-sized reference, 30 FPS. Actual boot firmware
# may need a framebuffer or frame-sequence conversion instead of MP4.
subprocess.run([
    'ffmpeg','-y','-loglevel','error','-i',str(OUT_MP4),'-vf','scale=320:320:flags=lanczos',
    '-an','-r','30','-c:v','libx264','-preset','slow','-crf','19',
    '-pix_fmt','yuv420p','-movflags','+faststart',str(OUT_MP4_320)
],check=True)
print('Wrote',OUT_MP4_320,OUT_MP4_320.stat().st_size)

# The preview ends with a short fade to black to make GIF looping unobtrusive.
# Use fps=15 and 360px to control size while retaining the art style.
filter_complex='[0:v]fps=15,scale=360:360:flags=lanczos,fade=t=out:st=6.55:d=0.45,split[s0][s1];[s0]palettegen=max_colors=192:stats_mode=diff[p];[s1][p]paletteuse=dither=sierra2_4a[v]'
subprocess.run([
    'ffmpeg','-y','-loglevel','error','-i',str(OUT_MP4),
    '-filter_complex',filter_complex,'-map','[v]','-loop','0',str(OUT_GIF)
],check=True)
print('Wrote',OUT_GIF,OUT_GIF.stat().st_size)
