"""Builds docs/Aftershock-Manual.pdf in the plugin's own look.

    python docs/manual/make_manual.py SHOTS_DIR

SHOTS_DIR holds the PNGs written by aftershock_snapshot.
"""
import json, sys, os
from reportlab.lib.pagesizes import A4
from reportlab.pdfgen import canvas
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.lib.colors import HexColor, Color
from reportlab.lib.utils import ImageReader
from reportlab.platypus import Paragraph, Table, TableStyle, Frame
from reportlab.lib.styles import ParagraphStyle

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
SHOTS = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'docs', 'shots')
FONTS = os.path.join(os.path.dirname(__file__), 'fonts')
pdfmetrics.registerFont(TTFont('Pix', os.path.join(ROOT, 'plugin', 'data', 'PixelifySans.ttf')))
pdfmetrics.registerFont(TTFont('Plex', os.path.join(FONTS, 'IBMPlexSans.ttf')))

W, H = A4
M = 54
BG, CARD = HexColor('#12101b'), HexColor('#1c1929')
T1, T2, T3 = HexColor('#f1effa'), HexColor('#c4c1d6'), HexColor('#8d8aa3')
ACC = dict(lime='#e4f58c', teal='#68c7d2', green='#77b99a', peri='#8ba9ec', grey='#b7c1ce', lav='#c798ee', pink='#db93c0', rose='#e8899b', yellow='#e9cb72', orange='#eda967')
A = {k: HexColor(v) for k, v in ACC.items()}
STRIPE = ['teal', 'orange', 'lav', 'rose', 'yellow', 'peri', 'green', 'grey', 'pink']

body = ParagraphStyle('b', fontName='Plex', fontSize=10, leading=15, textColor=T2)
small = ParagraphStyle('s', parent=body, fontSize=8.5, leading=12, textColor=T3)
cell = ParagraphStyle('c', parent=body, fontSize=8.6, leading=11.5)
cellh = ParagraphStyle('ch', parent=cell, fontName='Pix', textColor=T1)

version = '1.0.0'
for line in open(os.path.join(ROOT, 'plugin', 'CMakeLists.txt')):
    if 'project(Aftershock VERSION' in line:
        version = line.split('VERSION')[1].split()[0]

c = canvas.Canvas(os.path.join(ROOT, 'docs', 'Aftershock-Manual.pdf'), pagesize=A4)
c.setTitle('Aftershock manual'); c.setAuthor('Sofia Vallejo Budziszewski'); c.setSubject('A reverb made from earthquakes')
page = [0]

def background(accent='lav', accent2='teal'):
    c.setFillColor(BG); c.rect(0, 0, W, H, stroke=0, fill=1)
    # soft glows: many thin steps so the fade has no bands
    for i in range(60):
        t = i / 60
        c.setFillColor(Color(*[x / 255 for x in bytes.fromhex(ACC[accent][1:])], alpha=0.0045))
        c.circle(W * 0.12, H * 1.02, 560 * (1 - t), stroke=0, fill=1)
        c.setFillColor(Color(*[x / 255 for x in bytes.fromhex(ACC[accent2][1:])], alpha=0.0035))
        c.circle(W * 1.05, -20, 520 * (1 - t), stroke=0, fill=1)

def footer():
    page[0] += 1
    c.setFont('Pix', 8); c.setFillColor(T3)
    c.drawString(M, 28, 'AFTERSHOCK  ·  MANUAL  ·  v' + version)
    c.drawRightString(W - M, 28, str(page[0]))

def stripe(x, y, w, h=4):
    gap = 4; sw = (w - gap * 8) / 9
    for i, k in enumerate(STRIPE):
        c.setFillColor(A[k]); c.roundRect(x + i * (sw + gap), y, sw, h, h / 2, stroke=0, fill=1)

def title(text, accent='lime', sub=None):
    y = H - M - 10
    c.setFillColor(A[accent]); c.circle(M + 4, y + 7, 4, stroke=0, fill=1)
    c.setFont('Pix', 9); c.setFillColor(T3)
    c.drawString(M + 16, y + 4, (sub or '').upper())
    c.setFont('Pix', 26); c.setFillColor(T1)
    c.drawString(M, y - 30, text)
    return y - 52

def para(text, y, style=body, x=M, w=None):
    p = Paragraph(text, style); w = w or (W - 2 * M)
    _, h = p.wrap(w, H); p.drawOn(c, x, y - h); return y - h - 8

def heading(text, y, accent='lime'):
    c.setFillColor(A[accent]); c.circle(M + 3.5, y - 6, 3.5, stroke=0, fill=1)
    c.setFont('Pix', 12); c.setFillColor(T1); c.drawString(M + 14, y - 10, text)
    return y - 24

def card(x, y, w, h, accent='lav'):
    c.setFillColor(CARD); c.roundRect(x, y, w, h, 12, stroke=0, fill=1)
    col = A[accent]
    for i in range(30):
        c.setFillColor(Color(col.red, col.green, col.blue, alpha=0.012))
        c.roundRect(x, y + h - (h * 0.6) * (i + 1) / 30, w, (h * 0.6) * (i + 1) / 30, 12, stroke=0, fill=1)
    c.setStrokeColor(Color(1, 1, 1, alpha=0.08)); c.roundRect(x, y, w, h, 12, stroke=1, fill=0)

def shot(name, y, w=None):
    p = os.path.join(SHOTS, name + '.png'); img = ImageReader(p); iw, ih = img.getSize()
    w = w or (W - 2 * M); h = w * ih / iw
    c.setFillColor(Color(0, 0, 0, alpha=0.35)); c.roundRect(M + 3, y - h - 5, w, h, 10, stroke=0, fill=1)
    c.drawImage(img, M, y - h, w, h, mask='auto')
    return y - h - 14

def table(rows, y, widths, accent='lime'):
    data = [[Paragraph(str(v), cellh if i == 0 else cell) for v in r] for i, r in enumerate(rows)]
    t = Table(data, colWidths=widths)
    t.setStyle(TableStyle([
        ('VALIGN', (0, 0), (-1, -1), 'TOP'),
        ('LINEBELOW', (0, 0), (-1, 0), 0.8, A[accent]),
        ('LINEBELOW', (0, 1), (-1, -1), 0.3, Color(1, 1, 1, alpha=0.08)),
        ('TOPPADDING', (0, 0), (-1, -1), 4), ('BOTTOMPADDING', (0, 0), (-1, -1), 4),
        ('LEFTPADDING', (0, 0), (-1, -1), 4)]))
    _, h = t.wrap(W - 2 * M, H); t.drawOn(c, M, y - h); return y - h - 12

def newpage(a='lav', b='teal'):
    footer(); c.showPage(); background(a, b)

# ------------------------------------------------------------ cover
background('lav', 'teal')
c.setFont('Pix', 54); c.setFillColor(T1); c.drawString(M, H - 170, 'AFTERSHOCK')
stripe(M, H - 192, W - 2 * M, 6)
c.setFont('Pix', 16); c.setFillColor(A['lime']); c.drawString(M, H - 226, 'A reverb made from earthquakes')
c.setFont('Plex', 11); c.setFillColor(T2)
c.drawString(M, H - 248, 'User manual  ·  version ' + version + '  ·  VST3, AU and Standalone for macOS and Windows')
shot('simple_dark', H - 290)
y = 210
y = para('Every impulse response in Aftershock is a real seismogram: ground motion recorded by a real station after a real earthquake, '
         'squeezed in time so that the way the Earth rings and fades becomes your reverb tail. Thirty-five earthquakes, nine tectonic settings, '
         'up to three distances each.', y)
c.setFont('Pix', 10); c.setFillColor(T1); c.drawString(M, 96, 'Sofia Vallejo Budziszewski')
c.setFont('Plex', 9); c.setFillColor(T3)
c.drawString(M, 80, 'sofiavallejob.github.io/sofiavallejo  ·  github.com/sofiavallejob/aftershock')

# ------------------------------------------------------------ contents + install
newpage('teal', 'lav')
y = title('Contents', 'lime', 'start here')
toc = [('1', 'Installing', 'lime'), ('2', 'Quick start', 'teal'), ('3', 'The simple view', 'peri'), ('4', 'The advanced view', 'lav'),
       ('5', 'Every parameter', 'pink'), ('6', 'Presets', 'orange'), ('7', 'Toolbar, keys and settings', 'yellow'),
       ('8', 'Accessibility', 'green'), ('9', 'How it works', 'rose'), ('10', 'Credits and licence', 'grey')]
for n, t, a in toc:
    c.setFillColor(A[a]); c.circle(M + 4, y - 4, 4, stroke=0, fill=1)
    c.setFont('Pix', 12); c.setFillColor(T1); c.drawString(M + 18, y - 8, n + '   ' + t); y -= 22
y -= 14
y = heading('1  Installing', y, 'lime')
y = para('<b>macOS</b> (11 Big Sur or newer, Apple Silicon and Intel). Open <font color="#e4f58c">Aftershock-' + version + '-macOS.pkg</font> and follow the steps. '
         'You can choose the VST3 plug-in, the Audio Unit and the standalone app. They go to /Library/Audio/Plug-Ins/VST3, /Library/Audio/Plug-Ins/Components and /Applications. '
         'The installer is not signed with an Apple Developer ID, so the first time macOS may refuse to open it: right-click the .pkg, choose Open, then Open again.', y)
y = para('<b>Windows</b> (10 or 11, 64-bit). Run <font color="#e4f58c">Aftershock-' + version + '-Windows-Setup.exe</font>. The VST3 goes to C:\\Program Files\\Common Files\\VST3, '
         'the standalone app to Program Files\\Aftershock. Windows SmartScreen may warn that the publisher is unknown: choose More info, then Run anyway.', y)
y = para('<b>Without an installer.</b> The zip files hold the same plug-ins. Copy Aftershock.vst3 (and Aftershock.component on a Mac) into the folders above, or into your user plug-in folder.', y)
y = para('<b>In your DAW.</b> Rescan plug-ins. In Reaper: Options, Preferences, Plug-ins, VST, Re-scan. Aftershock appears under Effects, Reverb, by Sofia Vallejo. '
         'In Logic, the Audio Unit is under Audio Units, Sofia Vallejo.', y)

# ------------------------------------------------------------ quick start
newpage('lav', 'teal')
y = title('Quick start', 'teal', 'five minutes')
steps = [('Put it on a send or an insert', 'On a send, turn Dry / Wet to 100 % in the advanced view (Dry 0 %, Wet 100 %). On an insert, start around 25 to 35 % wet.'),
         ('Pick a preset', 'Click the preset name in the toolbar. The browser has 28 factory presets for vocals, drums, guitar and keys, synths and pads, mixing and sound design, plus an Earth Tour.'),
         ('Pick an earthquake', 'Click a coloured dot on the globe, or step with the arrows next to the earthquake name. Each colour is a tectonic setting.'),
         ('Choose the distance', 'Near stations are bright and busy. Regional ones are darker and smoother. Far ones have crossed a continent and are dark and long.'),
         ('Shape it', 'Length sets how long the tail is. Brightness lets more or less of the real spectrum through. Character blends in the raw, gritty seismogram. Pre-delay keeps the dry sound clear.')]
for i, (h, t) in enumerate(steps):
    card(M, y - 70, W - 2 * M, 64, list(ACC)[i + 1])
    c.setFont('Pix', 22); c.setFillColor(A[list(ACC)[i + 1]]); c.drawString(M + 16, y - 42, str(i + 1))
    c.setFont('Pix', 12); c.setFillColor(T1); c.drawString(M + 46, y - 24, h)
    para(t, y - 30, small, M + 46, W - 2 * M - 62)
    y -= 80
y -= 6
y = para('Hover any control to see what it does: help is on until you click the <font color="#68c7d2">i</font> in the toolbar. Double-click a knob to reset it. '
         'Hold Shift while dragging for fine steps. Everything you change can be undone.', y)

# ------------------------------------------------------------ simple view
newpage('peri', 'lav')
y = title('The simple view', 'peri', 'what the earth did, what you do with it')
y = shot('simple_dark', y)
y = para('The window has two halves. <font color="#c4c1d6"><b>The recording</b></font> (top) is the data: which earthquake, which station, what the ground did. '
         '<font color="#c4c1d6"><b>The reverb</b></font> (bottom) is what you do with it.', y)
rows = [['', 'What it is'],
        ['Globe', 'All 35 earthquakes. The lime ring is the one in use; the dashed line runs to the station that recorded it. Click a dot to switch, drag to turn the Earth.'],
        ['Listen from', 'Near, Regional or Far: three real stations at different distances from the same earthquake. Each button shows the distance.'],
        ['Earthquake', 'Name, magnitude, date, depth and tectonic setting. The arrows step through all of them; All 35 opens the full list grouped by setting.'],
        ['Seismogram', 'The north component of the real recording. The lime part, from the line on, becomes the reverb. The bars below show its energy over time.'],
        ['Time', 'Length of the tail, and pre-delay before it starts.'],
        ['Tone', 'Brightness (how much of the recording\'s own spectrum you hear) and Character (smooth envelopes, or the raw seismogram blended in).'],
        ['Space', 'Width of the tail, and Dry / Wet.'],
        ['Output', 'Input and output meters, and output gain.']]
y = table(rows, y, [92, W - 2 * M - 92], 'peri')

# ------------------------------------------------------------ advanced view
newpage('lav', 'peri')
y = title('The advanced view', 'lav', 'every parameter')
y = shot('advanced_dark', y)
rows = [['Panel', 'What it shows'],
        ['Earthquakes', 'World map with every earthquake, coloured by tectonic setting and sized by magnitude. Hover for the name, click to use it.'],
        ['Recording', 'The earthquake list, the station distance, and the station itself: network and code, channels, distance, signal-to-noise ratio, data centre, and how many times faster the recording is played.'],
        ['Ground velocity', 'All three components: vertical (Z), north (N, left channel) and east (E, right channel). Dashed lines mark the P, S and surface waves.'],
        ['Impulse response', 'The reverb your settings make, as a spectrogram from 60 Hz to 18 kHz.'],
        ['Coda fingerprint', 'Measured on the recording: energy in ten bands from 0.1 to 8 Hz (top), and how long each band takes to fade by 60 dB, the seismic RT60 (bottom). Faint dots are rougher fits.']]
y = table(rows, y, [100, W - 2 * M - 100], 'lav')

# ------------------------------------------------------------ parameters
newpage('lav', 'teal')
y = title('Every parameter', 'pink', 'all automatable')
rows = [['Parameter', 'Range', 'What it does'],
        ['Earthquake', '35 events', 'Which recording becomes the reverb.'],
        ['Station', 'Near, Regional, Far', 'Distance from the earthquake. Missing distances fall back to the closest one recorded.'],
        ['Window', 'Full, P, S, Surface', 'Which part of the recording is used: all of it, or starting at the P, S or surface waves.'],
        ['Channels', 'N|E, Z, all', 'Which ground motion feeds left and right.'],
        ['Length', '0.3 to 12 s', 'How long the tail is. The window is squeezed into this time, usually 100 to 700 times faster.'],
        ['Damping', '0 to 100 %', 'Extra high-frequency decay on top of the data.'],
        ['Spectrum', '0 to 100 %', 'How much of the recording\'s band levels survive. 100 % is as recorded. In the simple view this is Brightness, the other way round.'],
        ['Decay contrast', '0 to 3x', 'How differently the bands fade: 1x is the data, 0 makes them alike, above 1 exaggerates.'],
        ['Grit', '0 to 100 %', 'Blends in the raw seismogram played faster (audification). Character in the simple view.'],
        ['Reverse', 'on, off', 'Plays the impulse response backwards.'],
        ['Early arrivals', '0 to 100 %', 'Early reflections from the eight strongest bursts between the P and S waves.'],
        ['Early spread', '10 to 150 ms', 'Time span of the early arrivals.'],
        ['Pre-delay', '0 to 250 ms', 'Gap before the tail.'],
        ['Low cut', '20 to 600 Hz', '24 dB/oct high-pass on the tail.'],
        ['Tilt', 'dark to bright', 'Tilt EQ on the tail, shelves at 600 Hz and 2.5 kHz.'],
        ['Modulation', '0 to 100 %', 'Slow pitch movement in the tail.'],
        ['Width', '0 to 200 %', 'Mid/side width of the tail.'],
        ['Ducking', '0 to 100 %', 'Lowers the tail while you play, lets it bloom in the gaps.'],
        ['Freeze', 'on, off', 'Holds the current tail.'],
        ['Dry, Wet', '0 to 100 %', 'Levels of your signal and of the reverb.'],
        ['Output gain', '-24 to +12 dB', 'Overall level.'],
        ['Bypass', 'on, off', 'Host bypass, with a short crossfade.']]
y = table(rows, y, [90, 92, W - 2 * M - 182], 'pink')
y = para('Aftershock reports zero latency. The impulse response is re-rendered in the background about 80 ms after you stop moving a control, so the sound never drops out.', y, small)

# ------------------------------------------------------------ presets
newpage('lav', 'teal')
y = title('Presets', 'orange', '28 factory presets and your own')
y = shot('presets', y, (W - 2 * M) * 0.78)
y = para('Click the preset name in the toolbar to open the browser. Search by name, earthquake or place. Factory, User and Favourites are tabs; the star marks a favourite. '
         'The arrows next to the name step through all presets. <b>Save</b> stores the current settings as your own preset; <b>Load</b> opens a .aftershock file from anywhere. '
         'User presets are small text files in Documents/Aftershock/Presets, so you can share them by email.', y)
pre = json.load(open(os.path.join(ROOT, 'plugin', 'presets', 'factory_presets.json')))
for half in (0, 1):
    newpage('orange', 'lav')
    y = title('Factory presets' + (' (continued)' if half else ''), 'orange', 'what each one is for')
    rows = [['Preset', 'Category', 'Earthquake', 'About']]
    for p in pre['presets'][half * 14:(half + 1) * 14]:
        rows.append([p['name'], p['category'], p['event'] + ' · ' + {'tele': 'far'}.get(p['station'], p['station']), p['about']])
    y = table(rows, y, [92, 72, 96, W - 2 * M - 260], 'orange')

# ------------------------------------------------------------ toolbar + settings
newpage('lav', 'teal')
y = title('Toolbar, keys and settings', 'yellow', 'the top row')
rows = [['Control', 'What it does'],
        ['AFTERSHOCK', 'Opens About: version, credits, links and How it works.'],
        ['Preset', 'Previous and next preset, the preset browser, Save and Load.'],
        ['Compare A | B', 'Two independent settings. Pressing B the first time copies A. Changes go to the lit slot.'],
        ['Undo, Redo', 'Every change, including presets and earthquakes. Cmd+Z and Shift+Cmd+Z (Ctrl on Windows).'],
        ['i', 'Help on hover, on or off.'],
        ['Half circle', 'Light or dark.'],
        ['Gear', 'Settings.'],
        ['Advanced, Simple', 'Switch view.'],
        ['Power', 'Bypass. Lit means Aftershock is on.']]
y = table(rows, y, [100, W - 2 * M - 100], 'yellow')
y = shot('settings', y, (W - 2 * M) * 0.72)
y = para('<b>Settings</b> are saved for all instances: theme (dark, light or match the system), window size (75 to 200 %, or drag the corner), text size, '
         'colour vision, high contrast, reduced motion, spoken changes, focus outline, help, and whether new instances open with your last preset.', y)

# ------------------------------------------------------------ accessibility
newpage('lav', 'teal')
y = title('Accessibility', 'green', 'for everyone who makes music')
y = para('<b>Screen readers.</b> Every control has a name, a role and a spoken value, and works with VoiceOver on macOS and Narrator or NVDA on Windows: '
         'for example, "Length, 2.8 seconds, adjustable". The globe, map and graphs have text descriptions of what they show. With Announce changes on, new values are spoken as you change them.', y)
y = para('<b>Keyboard.</b> Tab and Shift+Tab move between controls. Arrow keys change a knob, Shift+arrow in fine steps, Return presses a button, Escape closes a window.', y)
y = para('<b>Colour vision.</b> Red-green safe uses the Okabe-Ito palette, which stays distinct for protanopia and deuteranopia. Blue-yellow safe is for tritanopia. '
         'Monochrome uses only brightness. Colours are never the only clue: every setting, slot and state is also named.', y)
y = para('<b>High contrast</b> uses a black background, solid white outlines and no fades. <b>Reduce motion</b> stops the globe; it also follows your system setting.', y)
y = para('<b>Size.</b> Window size up to 200 % and three text sizes.', y)
y = shot('simple_hc', y, (W - 2 * M) * 0.82)
c.setFont('Plex', 8.5); c.setFillColor(T3); c.drawString(M, y + 4, 'High contrast with the red-green safe palette.')

# ------------------------------------------------------------ how it works
newpage('lav', 'teal')
y = title('How it works', 'rose', 'from a seismometer to your reverb tail')
how = [('Real recordings', 'teal', '35 earthquakes chosen to cover nine tectonic settings. For each, up to three permanent broadband seismometers: near (under about 330 km), regional (3 to 15 degrees) and far (20 to 60 degrees). Waveforms from the FDSN data centres EarthScope, ORFEUS and GEOFON via ObsPy; origins from USGS ComCat.'),
       ('Cleaning', 'orange', 'Instrument response removed (0.02 to 9.5 Hz) to get true ground velocity, rotated to vertical, north and east, low-passed at 8 Hz and resampled to 20 Hz. Clipped sensors, sensor steps, energy before the P wave, gaps and signal-to-noise below 8 are rejected. Near stations that clip fall back to the accelerometer at the same site.'),
       ('Measurements', 'lav', 'P and S arrival times from the iasp91 Earth model. Ten bands from 0.1 to 8 Hz: energy, and the seismic RT60 fitted like T20 in room acoustics. The eight strongest bursts between P and S become the early arrivals.'),
       ('Time', 'yellow', 'The chosen window of the recording is squeezed into the reverb Length, typically 100 to 700 times faster.'),
       ('Frequency', 'peri', 'Each seismic band drives band-passed noise in a matching audio band from 120 Hz to 12 kHz, log onto log. So the reverb inherits each band\'s onset, peaks and decay: low still fades slowest, high fastest. North is left, east is right.'),
       ('Engine', 'green', 'Zero-latency partitioned convolution, then pre-delay, early arrivals, 24 dB/oct low cut, tilt, modulation, mid/side width, freeze and ducking.')]
for i, (h, a, t) in enumerate(how):
    p = Paragraph(t, small); _, ph = p.wrap(W - 2 * M - 40, H)
    hh = ph + 34
    card(M, y - hh, W - 2 * M, hh, a)
    c.setFont('Pix', 11); c.setFillColor(A[a]); c.drawString(M + 16, y - 20, str(i + 1) + '  ' + h)
    p.drawOn(c, M + 16, y - 26 - ph)
    y -= hh + 8
y -= 4
rows = [['Group (measured on the IRs)', 'n', 'Centroid', 'T20'],
        ['near (< 330 km)', '19', '2887 Hz', '3.6 s'], ['regional', '26', '1601 Hz', '5.7 s'], ['far', '35', '1058 Hz', '3.9 s'],
        ['volcanic', '3', '1916 Hz', '1.4 s'], ['deep slab', '2', '1535 Hz', '7.3 s'], ['stable craton', '5', '2968 Hz', '6.1 s']]
y = table(rows, y, [190, 40, 80, 60], 'rose')
y = para('Distance darkens the reverb, as attenuation predicts; volcanic recordings give short tails; deep-slab and craton paths the longest. The groups are small and the setting is mixed up with magnitude and station, '
         'so these are reasons for a listening test, not results. The full method is in docs/SCIENCE.md.', y, small)

# ------------------------------------------------------------ credits
newpage('lav', 'teal')
y = title('Credits and licence', 'grey', 'thank you')
y = para('<b>Made by Sofia Vallejo Budziszewski.</b> Website: sofiavallejob.github.io/sofiavallejo  ·  Source: github.com/sofiavallejob/aftershock', y)
y = para('<b>Data.</b> Seismograms from the FDSN data centres EarthScope, ORFEUS and GEOFON. Every recording shows its FDSN network code in the plugin; network citations and DOIs are at fdsn.org/networks. '
         'Earthquake origins: USGS ComCat. Travel times: iasp91 via ObsPy TauP. Coastlines: Natural Earth (public domain).', y)
y = para('<b>Software and fonts.</b> Built with JUCE 8. Interface font Pixelify Sans by Stefie Justprince (SIL Open Font License 1.1). Manual set in Pixelify Sans and IBM Plex Sans (SIL OFL).', y)
y = para('<b>Sound examples</b> use freely licensed recordings from Freesound; their authors are listed in examples/CREDITS.md.', y)
y = para('<b>Licence.</b> Aftershock is free software under the GNU Affero General Public License v3.0. You can use it in any music, commercial or not: the licence covers the code, not what you make with it. '
         'If you share a modified version, share its source under the same licence.', y)
y -= 6
card(M, y - 72, W - 2 * M, 72, 'lime')
c.setFont('Pix', 13); c.setFillColor(A['lime']); c.drawString(M + 16, y - 24, 'Free to port, by humans. Bring your own ears.')
para('Ports and forks are welcome, with credit and under the same licence. Please do not use this code to train machine-learning models.', y - 32, small, M + 16, W - 2 * M - 32)
footer(); c.save()
print('wrote docs/Aftershock-Manual.pdf,', page[0], 'pages')
