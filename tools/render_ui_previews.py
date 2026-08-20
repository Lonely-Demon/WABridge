from PIL import Image, ImageDraw, ImageFont
from pathlib import Path

OUT = Path('/home/ubuntu/WABridge/artifacts/ui-previews')
OUT.mkdir(parents=True, exist_ok=True)


def font(size, bold=False):
    candidates = [
        '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf' if bold else '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',
        '/usr/share/fonts/truetype/liberation2/LiberationSans-Bold.ttf' if bold else '/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf',
    ]
    for path in candidates:
        if Path(path).exists():
            return ImageFont.truetype(path, size)
    return ImageFont.load_default()


def rounded(draw, box, fill, outline=None, radius=10, width=1):
    draw.rounded_rectangle(box, radius=radius, fill=fill, outline=outline, width=width)


def draw_windows():
    w, h = 1120, 760
    bg = '#e9edf2'
    im = Image.new('RGB', (w, h), bg)
    d = ImageDraw.Draw(im)
    title = font(20, True); body = font(16); small = font(13); button = font(15, True)

    # Window frame and title bar
    rounded(d, (90, 54, 1030, 706), '#ffffff', '#aeb8c4', 12, 2)
    d.rectangle((91, 55, 1029, 103), fill='#f4f6f8')
    d.text((118, 69), 'WABridge — Windows coordinator', font=title, fill='#17212b')
    d.text((960, 69), '—  □  ×', font=body, fill='#596775')

    # Form
    x0, x1 = 130, 990
    d.text((130, 132), 'Device certificate:', font=body, fill='#26333e')
    rounded(d, (320, 124, 850, 162), '#ffffff', '#b9c4cf', 6)
    d.text((338, 134), 'Optional PEM certificate override', font=small, fill='#8b98a5')
    rounded(d, (865, 124, 970, 162), '#f2f5f7', '#b9c4cf', 6)
    d.text((882, 134), 'Browse…', font=small, fill='#26333e')

    d.text((130, 184), 'Private key:', font=body, fill='#26333e')
    rounded(d, (320, 176, 850, 214), '#ffffff', '#b9c4cf', 6)
    d.text((338, 186), 'Optional PEM private-key override', font=small, fill='#8b98a5')
    rounded(d, (865, 176, 970, 214), '#f2f5f7', '#b9c4cf', 6)
    d.text((882, 186), 'Browse…', font=small, fill='#26333e')

    d.text((130, 236), 'TCP port:', font=body, fill='#26333e')
    rounded(d, (320, 228, 470, 266), '#ffffff', '#b9c4cf', 6)
    d.text((338, 238), '51820', font=body, fill='#26333e')

    # Status
    rounded(d, (130, 294, 970, 360), '#edf6ff', '#b7d5ef', 8)
    d.text((152, 311), 'Not running — a DPAPI-protected device identity will be created automatically', font=small, fill='#23547a')

    # Buttons
    def btn(x, label, enabled=True, width=220):
        fill = '#1264a3' if enabled else '#d9dee3'
        textfill = '#ffffff' if enabled else '#87919a'
        rounded(d, (x, 390, x + width, 438), fill, '#0d4f82' if enabled else '#c3cbd2', 7)
        d.text((x + 16, 397), label, font=button, fill=textfill)
    btn(130, 'Start secure coordinator', True, 250)
    btn(394, 'Stop', False, 115)
    btn(525, 'Start Phone Control', False, 220)

    # Note
    d.multiline_text((130, 490),
        'WABridge uses TLS 1.3 and mutual certificate authentication.\n'
        'The default identity is stored with Windows DPAPI; PEM fields are optional test overrides.\n'
        'A first-pair connection remains pending until the device identities are compared.',
        font=body, fill='#485764', spacing=9)
    d.text((130, 630), 'Preview of the implemented Qt shell — runtime status and buttons change after connection.', font=small, fill='#7a8792')
    im.save(OUT / 'wabridge-windows-ui-preview.png')


def draw_android():
    w, h = 700, 1280
    im = Image.new('RGB', (w, h), '#f7f8fa')
    d = ImageDraw.Draw(im)
    title = font(32, True); subtitle = font(18); body = font(17); label = font(15); button = font(16, True)

    # Phone frame
    rounded(d, (42, 22, 658, 1258), '#ffffff', '#6f7b86', 42, 4)
    rounded(d, (67, 78, 633, 1205), '#fbfcfe', '#d7dde3', 22, 2)
    d.rounded_rectangle((275, 43, 425, 65), radius=12, fill='#20252a')

    d.text((98, 112), 'WABridge', font=title, fill='#17212b')
    d.text((98, 160), 'Windows–Android workspace bridge', font=subtitle, fill='#52616d')
    d.text((98, 212), 'State: DISCONNECTED', font=font(18, True), fill='#a14925')
    d.text((98, 250), 'No Windows coordinator connected', font=body, fill='#52616d')

    def btn(y, text, enabled=True):
        fill = '#1264a3' if enabled else '#d9dee3'
        txt = '#ffffff' if enabled else '#89939c'
        rounded(d, (98, y, 602, y + 52), fill, '#0d4f82' if enabled else '#c3cbd2', 9)
        d.text((124, y + 15), text, font=button, fill=txt)

    btn(292, 'Find Windows laptop', True)

    # Text fields
    def field(y, text, hint=False):
        rounded(d, (98, y, 602, y + 62), '#ffffff', '#aebbc7', 8, 2)
        d.text((120, y + 7), text, font=label, fill='#52616d')
        if not hint:
            d.text((120, y + 30), ' ', font=body, fill='#17212b')
    field(372, 'Windows IP or hostname')
    field(456, 'TCP port')
    btn(542, 'Connect manually', False)
    btn(614, 'Capture phone audio to Windows', False)
    btn(686, 'Stop session', True)

    # Pairing area (shown as an available state example)
    rounded(d, (98, 776, 602, 960), '#fff8e8', '#e4c579', 12, 2)
    d.text((122, 800), 'New Windows device requires approval', font=font(17, True), fill='#654b13')
    d.text((122, 846), 'Device: DESKTOP-EXAMPLE', font=body, fill='#654b13')
    d.text((122, 884), 'Fingerprint: 12:34:56:78:9A:BC', font=body, fill='#654b13')
    rounded(d, (122, 912, 350, 946), '#b9770e', '#925d05', 7)
    d.text((140, 918), 'Approve pairing', font=label, fill='#ffffff')

    d.multiline_text((98, 1005),
        'TLS 1.3 and certificate pinning protect the session.\n'
        'Phone Control and MediaProjection remain user-authorized features.',
        font=body, fill='#52616d', spacing=8)
    d.text((98, 1165), 'Preview of the implemented Compose shell. Pairing card is shown as a state example.', font=label, fill='#7a8792')
    im.save(OUT / 'wabridge-android-ui-preview.png')


def draw_contact():
    win = Image.open(OUT / 'wabridge-windows-ui-preview.png')
    android = Image.open(OUT / 'wabridge-android-ui-preview.png')
    scale = 0.48
    win = win.resize((int(win.width * scale), int(win.height * scale)))
    android = android.resize((int(android.width * scale), int(android.height * scale)))
    canvas = Image.new('RGB', (win.width + android.width + 90, max(win.height, android.height) + 80), '#dfe5eb')
    canvas.paste(win, (30, 40))
    canvas.paste(android, (win.width + 60, 40))
    canvas.save(OUT / 'wabridge-ui-contact-sheet.png')


draw_windows()
draw_android()
draw_contact()
