#!/usr/bin/env python3
"""Renders the launcher's wallpapers into res/wallpapers/ as JPEGs.

Each wallpaper is generated from scratch for this project: soft colour fields in the spirit of
recent Apple defaults, with no Apple artwork. 1024x768, drawn at 2x and scaled down, with a
little grain against banding. Bloom is the default. Needs Pillow and numpy:
    python3 tools/wallpapers.py
Design notes: TDD section 12.4.3, mockup 37.
"""
import numpy as np, os
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "res", "wallpapers")
os.makedirs(OUT, exist_ok=True)
from PIL import Image, ImageFilter
W,H=1024,768; S=2; w,h=W*S,H*S
yy,xx=np.mgrid[0:h,0:w].astype(np.float32); X=xx/w; Y=yy/h
rng=np.random.default_rng(7)
def hexc(s): s=s.lstrip('#'); return np.array([int(s[i:i+2],16) for i in (0,2,4)],np.float32)/255
def vgrad(a,b,c=None):
    a,b=hexc(a),hexc(b)
    if c is None: return a+(b-a)*Y[...,None]
    c=hexc(c); t=Y[...,None]; return np.where(t<.5,a+(b-a)*(t*2),b+(c-b)*((t-.5)*2))
def blob(img,cx,cy,r,col,a=1.0,sx=1.0):
    d=((X-cx)/sx)**2*(w/h)**2+(Y-cy)**2; k=np.exp(-d/(2*r*r))[...,None]*a
    return img*(1-k)+hexc(col)*k
def bands(img,cols,base,amp,freq,phase,soft=0.004):
    # layered hills: each band below a wavy edge takes its own colour, lit at the edge
    for i,(c1,c2) in enumerate(cols):
        edge=base[i]+amp[i]*np.sin(X*freq[i]*6.283+phase[i])+amp[i]*0.4*np.sin(X*freq[i]*2.1*6.283+phase[i]*1.7)
        m=np.clip((Y-edge)/soft,0,1)[...,None]
        t=np.clip((Y-edge)/0.35,0,1)[...,None]
        col=hexc(c1)+(hexc(c2)-hexc(c1))*t
        img=img*(1-m)+col*m
    return img
def finish(img,blur=0,grain=0.012,name=''):
    im=Image.fromarray((np.clip(img,0,1)*255).astype(np.uint8))
    if blur: im=im.filter(ImageFilter.GaussianBlur(blur*S))
    a=np.asarray(im).astype(np.float32)/255
    a+=rng.normal(0,grain,a.shape[:2])[...,None]
    im=Image.fromarray((np.clip(a,0,1)*255).astype(np.uint8)).resize((W,H),Image.LANCZOS)
    path=os.path.join(OUT,name+'.jpg'); im.save(path,quality=90,optimize=True); print(path)
    # a small, already blurred copy for backgrounds such as Settings, so the device skips the blur
    small=im.filter(ImageFilter.GaussianBlur(24)).resize((256,192),Image.LANCZOS)
    small.save(os.path.join(OUT,name+'-blur.jpg'),quality=90,optimize=True)
    return im
out={}
# 1 Dusk hills: warm layered hills at sunset
img=vgrad('#2b1d4f','#d9677a','#f4a46a')
img=bands(img,[('#6a3d7a','#2a1940'),('#b04a6e','#3a1d4a'),('#e07a5f','#4a2040'),('#7a2e5a','#1e1030')],
          [0.42,0.55,0.66,0.80],[0.035,0.04,0.03,0.035],[0.8,1.1,0.7,1.3],[0.5,2.0,4.0,1.0],0.003)
out['dusk-hills']=finish(img,1.5,name='dusk-hills')
# 2 Bloom: blue and violet light on near-black, like a glass glow
img=vgrad('#05060f','#0a0c1c')
for cx,cy,r,c,a in ((0.30,0.62,0.22,'#2f6bff',.9),(0.62,0.40,0.20,'#8a4dff',.85),(0.78,0.75,0.16,'#ff4fa8',.6),(0.45,0.30,0.12,'#4fd2ff',.5)):
    img=blob(img,cx,cy,r,c,a)
out['bloom']=finish(img,30,name='bloom')
# 3 Dunes: pink and blue sand curves, daylight
img=vgrad('#7fb2ff','#f6c1d6')
img=bands(img,[('#f3a6c4','#c06aa0'),('#d98bb8','#7d4a8c'),('#9a6bc0','#4b3a7a'),('#5a4a9a','#241c4a')],
          [0.38,0.52,0.67,0.82],[0.05,0.045,0.04,0.03],[0.6,0.9,0.7,1.0],[3.0,0.4,2.2,5.0],0.003)
out['dunes']=finish(img,1.5,name='dunes')
# 4 Aurora: green and teal curtains over a dark sky
img=vgrad('#020812','#041a24')
for k,(base,amp,col,a) in enumerate(((0.52,0.07,'#2bf0a8',0.9),(0.44,0.05,'#22c8e0',0.6),(0.60,0.04,'#7affc0',0.45))):
    edge=base+amp*np.sin(X*4.0+k*1.9)+amp*0.5*np.sin(X*9.0+k)
    up=np.clip((edge-Y)/0.28,0,1); below=np.clip((Y-edge)/0.015,0,1)
    streak=0.65+0.35*np.sin(X*70+k*3)*np.sin(X*23+k)
    inten=(np.exp(-3.2*up)*(1-below)*(Y<edge+0.015))*streak*a
    img=img+hexc(col)*inten[...,None]*0.75
out['aurora']=finish(img,6,name='aurora')
# 5 Graphite: dark grey with one soft light ribbon
img=vgrad('#1b1c20','#0d0e10')
edge=0.55+0.12*np.sin(X*3.4+0.6); d=np.abs(Y-edge); k=np.exp(-(d/0.05)**2)[...,None]*0.35
img=img*(1-k)+hexc('#c8ccd6')*k
out['graphite']=finish(img,8,name='graphite')
# 6 Ember: orange and red glow from the lower right
img=vgrad('#14080a','#2a0d0c')
for cx,cy,r,c,a in ((0.85,0.95,0.38,'#ff6a1f',.95),(0.60,0.80,0.22,'#ff2d55',.6),(0.95,0.55,0.18,'#ffb340',.5)):
    img=blob(img,cx,cy,r,c,a)
out['ember']=finish(img,30,name='ember')
# 7 Tide: deep blue waves
img=vgrad('#0a2a5a','#04102a')
img=bands(img,[('#1f5fbf','#0a2a6a'),('#1a4aa0','#081e50'),('#123a82','#06163c'),('#0c2a62','#040c24')],
          [0.45,0.58,0.70,0.83],[0.03,0.03,0.025,0.025],[1.4,1.8,1.2,2.0],[0.0,1.5,3.0,4.5],0.004)
out['tide']=finish(img,2,name='tide')
# 8 Lagoon: teal and aqua light, calm and bright
img=vgrad('#0b3b4a','#062530')
for cx,cy,r,c,a in ((0.30,0.35,0.24,'#2fd6c8',.75),(0.72,0.30,0.20,'#4fa8ff',.6),(0.55,0.80,0.26,'#0a8a8a',.7),(0.15,0.85,0.14,'#7af0d8',.4)):
    img=blob(img,cx,cy,r,c,a)
out['lagoon']=finish(img,30,name='lagoon')
