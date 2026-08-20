from PIL import Image, ImageDraw, ImageFont
from pathlib import Path

OUT = Path('/home/ubuntu/WABridge/artifacts/ui-previews')
OUT.mkdir(parents=True, exist_ok=True)
BG = '#111317'; CARD = '#1b1f27'; HERO = '#1d2d51'; MUTED = '#9aa6b6'; WHITE = '#f7f9fc'; BLUE = '#4b8eff'; CYAN = '#5de6ff'; GOLD = '#ffd60a'; GREEN = '#55e68b'; ORANGE = '#ef6719'; STROKE = '#2c3440'

def f(size, bold=False):
    paths = ['/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf' if bold else '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf']
    return ImageFont.truetype(paths[0], size)

def rr(d, box, fill, outline=None, radius=14, width=1):
    d.rounded_rectangle(box, radius=radius, fill=fill, outline=outline, width=width)

def text(d, xy, s, size=16, fill=WHITE, bold=False, anchor=None):
    d.text(xy, s, font=f(size, bold), fill=fill, anchor=anchor)

def windows():
    im = Image.new('RGB', (1240, 820), BG); d = ImageDraw.Draw(im)
    d.rectangle((0, 0, 226, 820), fill='#171a20'); d.line((226,0,226,820), fill='#2b313b', width=1)
    text(d,(34,36),'WABridge',25,WHITE,True); text(d,(35,73),'WINDOWS COORDINATOR',10,'#8390a3',True)
    navs=[('⌂','Dashboard',True),('◉','Phone Control',False),('♫','Audio',False),('□','Files',False),('▣','Clipboard',False),('⚙','Settings',False)]
    y=142
    for icon,label,sel in navs:
        if sel: rr(d,(22,y-8,205,y+35),'#22304a',radius=9)
        text(d,(38,y),icon,20,BLUE if sel else MUTED,False); text(d,(70,y+2),label,14,BLUE if sel else MUTED,sel); y+=52
    text(d,(30,735),'TLS 1.3',13,MUTED); text(d,(30,758),'Certificate pinning',13,MUTED); text(d,(30,781),'Wi-Fi only',13,MUTED)
    x=260; text(d,(x,32),'LOCAL WORKSPACE BRIDGE',10,'#8390a3',True); text(d,(x,60),'Your devices, working together',29,WHITE,True); text(d,(x,100),'Connect Android to Windows over your trusted Wi-Fi network.',14,MUTED)
    rr(d,(1058,38,1198,74),'#193a2a','#28633e',12); text(d,(1128,56),'●  OFFLINE',11,GREEN,True,'mm')
    rr(d,(x,142,1200,310),HERO,'#31588d',18,1); rr(d,(x+22,166,x+98,242),'#243e71',None,16); text(d,(x+60,204),'◫',34,'#72a6ff',True,'mm')
    text(d,(x+124,174),'Connect your Android phone',18,WHITE,True); text(d,(x+124,207),'Discover a nearby device or use the manual IP fallback.',13,MUTED); text(d,(x+124,230),'First pairing always requires fingerprint approval.',13,MUTED)
    rr(d,(x+124,258,x+330,294),'#1677e8','#3e9aff',9); text(d,(x+227,276),'Start secure coordinator',13,WHITE,True,'mm')
    rr(d,(x+344,258,x+478,294),'#202630','#3a4655',9); text(d,(x+411,276),'Stop',13,'#dbe5f1',True,'mm')
    rr(d,(x+492,258,x+670,294),'#202630','#3a4655',9); text(d,(x+581,276),'Start Phone Control',13,'#dbe5f1',True,'mm')
    cards=[('◫','Second Display','Android as an extra Windows screen','READY AFTER SESSION','#72a6ff'),('◉','Phone Control','Mouse and keyboard to Android','USER AUTHORIZATION REQUIRED',CYAN),('♫','Android Audio','Play phone audio on Windows','MEDIA PROJECTION REQUIRED',GREEN),('□','Files + Clipboard','Move content between devices','READY AFTER SESSION',GOLD)]
    positions=[(x,330),(x+468,330),(x,452),(x+468,452)]
    for (icon,name,desc,state,color),(cx,cy) in zip(cards,positions):
        rr(d,(cx,cy,cx+450,cy+104),CARD,STROKE,16); text(d,(cx+20,cy+22),icon,23,color,True); text(d,(cx+66,cy+18),name,15,WHITE,True); text(d,(cx+66,cy+44),desc,12,MUTED); text(d,(cx+66,cy+75),state,10,color,True)
    rr(d,(x,582,1200,678),CARD,STROKE,12); text(d,(x+18,604),'ADVANCED SECURITY AND NETWORK SETTINGS',11,MUTED,True); text(d,(x+18,634),'Certificate overrides and TCP port 51820 are available here when needed.',12,MUTED)
    rr(d,(x,696,1200,758),CARD,STROKE,12); text(d,(x+18,727),'Ready to connect — a DPAPI-protected Windows identity will be created automatically',12,MUTED)
    im.save(OUT/'wabridge-windows-ui-redesign-preview.png')

def android():
    im=Image.new('RGB',(720,1380),BG); d=ImageDraw.Draw(im); rr(d,(34,18,686,1362),'#0c0d10','#3b424d',38,3); rr(d,(62,76,658,1305),BG,'#252a33',22)
    rr(d,(285,40,435,62),'#000000',None,13); text(d,(96,112),'WABridge',30,WHITE,True); text(d,(98,150),'Windows–Android workspace bridge',15,MUTED); text(d,(98,192),'●  READY TO CONNECT',11,MUTED,True)
    rr(d,(98,228,622,555),HERO,'#31588d',20); rr(d,(313,252,407,346),'#243e71',None,18); text(d,(360,299),'◫',38,'#72a6ff',True,'mm'); text(d,(360,382),'Connect your Windows laptop',22,WHITE,True,'mm'); text(d,(360,415),'Second Display, Phone Control, files, clipboard,',14,MUTED,False,'mm'); text(d,(360,440),'and audio over Wi-Fi.',14,MUTED,False,'mm')
    rr(d,(122,474,598,520),'#1677e8','#3e9aff',14); text(d,(360,497),'Find Windows laptop',15,WHITE,True,'mm'); rr(d,(122,530,598,574),CARD,STROKE,14); text(d,(360,552),'⌕   Connect by IP address',14,'#dbe5f1',True,'mm')
    text(d,(98,598),'WORKSPACE FEATURES',11,MUTED,True); cards=[('◫','Second Display','Waiting for connection',BLUE),('◉','Phone Control','Waiting for connection',CYAN),('□','Files','Waiting for connection',GOLD),('▣','Clipboard','Waiting for connection',GREEN)]
    pos=[(98,624),(362,624),(98,758),(362,758)]
    for (icon,name,desc,color),(x,y) in zip(cards,pos):
        rr(d,(x,y,x+238,y+118),CARD,STROKE,18); text(d,(x+18,y+18),icon,23,color,True); text(d,(x+18,y+56),name,14,WHITE,True); text(d,(x+18,y+83),desc,11,MUTED)
    rr(d,(98,892,622,1002),CARD,STROKE,18); text(d,(120,916),'♫',25,CYAN,True); text(d,(165,918),'Phone audio',15,WHITE,True); text(d,(165,946),'Available after connection',12,MUTED); text(d,(594,944),'›',25,MUTED,False,'mm')
    rr(d,(98,1026,622,1122),CARD,STROKE,18); text(d,(120,1050),'▣',25,BLUE,True); text(d,(165,1050),'Encrypted by default',15,WHITE,True); text(d,(165,1078),'TLS 1.3 · certificate pinning · first-pair approval',11,MUTED)
    rr(d,(126,1180,594,1250),'#1c1c1e','#39414c',30); text(d,(192,1215),'▶  Connect',12,BLUE,True,'mm'); text(d,(354,1215),'▦  Features',12,MUTED,False,'mm'); text(d,(514,1215),'⚙',15,MUTED,False,'mm')
    text(d,(98,1274),'A polished dashboard for the Windows–Android workspace.',11,'#7f8996')
    im.save(OUT/'wabridge-android-ui-redesign-preview.png')

def contact():
    a=Image.open(OUT/'wabridge-android-ui-redesign-preview.png').resize((360,690)); w=Image.open(OUT/'wabridge-windows-ui-redesign-preview.png').resize((620,410)); c=Image.new('RGB',(1030,760),'#d9dee6'); c.paste(w,(25,55)); c.paste(a,(650,35)); c.save(OUT/'wabridge-ui-redesign-contact-sheet.png')

windows(); android(); contact()
