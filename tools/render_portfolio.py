"""Generate dependency-free SVG documentation from the actual map configuration."""
import json
from html import escape
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'docs/images'
CONFIG = json.loads((ROOT / 'data/config.json').read_text())
INK = '#eaf2ef'
MUTED = '#a4bbb2'
GREEN = '#a5e6b5'
BLUE = '#8ec7f7'
GOLD = '#f6c66d'


def text(x, y, value, size=20, color=INK, weight=400):
    return f'<text x="{x}" y="{y}" fill="{color}" font-size="{size}" font-weight="{weight}">{escape(str(value))}</text>'


def rect(x, y, w, h, fill='#142a24', stroke='#2d493e', radius=18):
    return f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{radius}" fill="{fill}" stroke="{stroke}"/>'


def line(x1, y1, x2, y2, color=GREEN, width=2, arrow=False):
    marker = ' marker-end="url(#arrow)"' if arrow else ''
    return f'<path d="M{x1},{y1} L{x2},{y2}" fill="none" stroke="{color}" stroke-width="{width}"{marker}/>'


def save(name, title, body, height=620):
    svg = f'''<svg xmlns="http://www.w3.org/2000/svg" width="1200" height="{height}" viewBox="0 0 1200 {height}" role="img" aria-labelledby="title">
<title id="title">{escape(title)}</title>
<defs><marker id="arrow" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse"><path d="M0 0L10 5L0 10Z" fill="{GREEN}"/></marker>
<pattern id="grid" width="32" height="32" patternUnits="userSpaceOnUse"><path d="M32 0H0V32" fill="none" stroke="#20392e" stroke-width="0.7"/></pattern></defs>
<rect width="1200" height="{height}" rx="24" fill="#0d1e19"/>
<g font-family="Arial, Helvetica, sans-serif">{body}</g></svg>'''
    (OUT / name).write_text(svg + '\n')


def map_drawing(m, x, y, w, h):
    points = [(r['x0'], r['y0']) for r in m['roads']]
    points += [(r.get('x1', r['x0']), r.get('y1', r['y0'])) for r in m['roads']]
    points += [(b['x'] + b['w'], b['y'] + b['h']) for b in m['buildings']]
    points += [(o['x'], o['y']) for o in m['offices']]
    xmin, xmax = min(p[0] for p in points), max(p[0] for p in points)
    ymin, ymax = min(p[1] for p in points), max(p[1] for p in points)
    scale = min((w - 56) / (xmax - xmin), (h - 56) / (ymax - ymin))
    ox = x + (w - (xmax - xmin) * scale) / 2
    oy = y + (h - (ymax - ymin) * scale) / 2
    def px(v): return round(ox + (v - xmin) * scale, 2)
    def py(v): return round(oy + (v - ymin) * scale, 2)
    result = rect(x, y, w, h, '#10271f')
    result += f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="18" fill="url(#grid)"/>'
    for b in m['buildings']:
        result += rect(px(b['x']), py(b['y']), round(b['w']*scale,2), round(b['h']*scale,2), '#2e5342', '#50705b', 3)
    for r in m['roads']:
        coords = (px(r['x0']), py(r['y0']), px(r.get('x1',r['x0'])), py(r.get('y1',r['y0'])))
        result += line(*coords, '#557464', max(6,scale*0.85))
        result += line(*coords, '#d0e1bc', 2)
    for o in m['offices']:
        a,b = px(o['x']),py(o['y'])
        result += f'<circle cx="{a}" cy="{b}" r="12" fill="{GOLD}" stroke="#0d1e19" stroke-width="3"/>'
        result += f'<path d="M{a-4} {b-2}L{a} {b+3}L{a+5} {b-4}" fill="none" stroke="#283323" stroke-width="2"/>'
    return result


OUT.mkdir(exist_ok=True)
body = text(56,62,'DOG STORY / C++ BACKEND',16,GREEN,700)
body += text(56,150,'Собрать. Доставить.',54,INK,700)
body += text(56,214,'Сохранить результат.',48,INK,700)
body += text(56,277,'Многопользовательская игра',24,MUTED)
body += text(56,311,'с серверной симуляцией мира.',24,MUTED)
for x,label,w in [(56,'C++20',106),(174,'Boost.Asio',154),(340,'PostgreSQL',166),(518,'Docker',108)]:
    body += rect(x,356,w,43) + text(x+16,384,label,18,GREEN,700)
body += text(56,465,'Движение · предметы · очки · восстановление',18,MUTED)
body += map_drawing(CONFIG['maps'][1],704,56,440,420)
body += text(715,509,'TOWN / схема реальной карты',16,MUTED)
save('cover.svg','Dog Story — C++ сервер, схема карты Town',body,550)

for i,m in enumerate(CONFIG['maps'],1):
    speed = m.get('dogSpeed',CONFIG['defaultDogSpeed'])
    body = text(48,55,f'WORLD / 0{i}',15,GREEN,700)
    body += text(48,108,m['name'],42,INK,700)
    body += text(48,147,f"id: {m['id']}",20,MUTED)
    body += map_drawing(m,436,42,716,530)
    stats = [(len(m['roads']),'сегменты дорог'),(len(m['buildings']),'здания'),(len(m['offices']),'пункт доставки'),(f'{speed:g}','скорость, ед./с')]
    for j,(value,label) in enumerate(stats):
        yy = 220+j*67
        body += text(48,yy,value,31,GREEN,700) + text(120,yy-2,label,19,MUTED)
    body += line(48,506,83,506,'#d0e1bc',5)+text(96,513,'дорога',17,MUTED)
    body += rect(48,530,24,18,'#2e5342','#50705b',3)+text(96,546,'здание',17,MUTED)
    body += f'<circle cx="60" cy="575" r="9" fill="{GOLD}"/>'+text(96,581,'пункт доставки',17,MUTED)
    body += text(436,606,'Геометрия из data/config.json · схематичный вид сверху',16,MUTED)
    save(f"map-{m['id']}.svg",f"{m['name']} — схема дорог, зданий и пунктов доставки",body,640)


def card(x,y,w,h,title,rows,accent=GREEN):
    result = rect(x,y,w,h) + text(x+22,y+37,title,24,accent,700)
    for n,row in enumerate(rows): result += text(x+22,y+72+n*27,row,18,MUTED)
    return result

body = text(48,52,'ARCHITECTURE / ОБРАБОТКА ЗАПРОСА',16,GREEN,700)
body += text(48,100,'От HTTP к игровой модели',38,INK,700)
body += card(48,148,250,136,'Контроллер',['Маршрут и авторизация','Валидация запроса'])
body += card(366,148,328,136,'Application',['Вход, команды, состояние','Координация сохранения'])
body += card(762,148,390,136,'Domain',['Game · Session · Dog · Map','Движение, сбор, очки'])
body += line(298,211,360,211,arrow=True)+line(694,211,756,211,arrow=True)
body += card(48,368,250,140,'JSON View',['Сериализация результата','JSON для HTTP-ответа'],BLUE)
body += line(172,284,172,360,arrow=True)
body += text(189,329,'ответ',16,MUTED)
body += card(366,368,328,140,'Repository',['Интерфейс домена','Рекорды завершённых игр'],GOLD)
body += line(530,284,530,360,arrow=True)
body += card(762,368,390,140,'PostgreSQL adapter',['Реализует Repository','SQL и подключение к БД'],GOLD)
body += line(762,438,702,438,arrow=True)
body += text(790,335,'реализация интерфейса',16,MUTED)
body += text(48,569,'HTTP-транспорт: Boost.Beast / Asio. Операции игры и тики выполняются в одном strand.',19,MUTED)
save('architecture.svg','Слои сервера и интерфейс Repository',body)

body = text(48,52,'PERSISTENCE / DOCKER COMPOSE',16,GREEN,700)
body += text(48,100,'Два вида данных — два хранилища',36,INK,700)
body += card(48,154,285,142,'Браузер',['Dog Story + REST API','localhost:8080'],BLUE)
body += card(414,154,738,142,'game / C++ server',['Игровая симуляция · HTTP API · статические файлы','Автоматические тики и периодические снимки'])
body += line(333,223,406,223,arrow=True)
body += card(48,402,490,142,'snapshots / Docker volume',['Положение, сумка, очки, токены','StateStore → файл game.state'],GREEN)
body += card(662,402,490,142,'db / PostgreSQL 16',['Результаты завершённых сессий','Repository → SQL → том database'],GOLD)
body += '<path d="M600 296V345H292V394" fill="none" stroke="'+GREEN+'" stroke-width="2" marker-end="url(#arrow)"/>'
body += line(909,296,909,394,arrow=True)
body += text(79,374,'Активная игра',19,GREEN,700)
body += text(687,374,'Таблица рекордов',19,GOLD,700)
body += text(48,598,'Тома сохраняются после docker compose down. База доступна только внутри сети Compose.',18,MUTED)
body += text(48,633,'Снимок и рекорд в БД не объединены общей транзакцией.',17,MUTED)
save('persistence.svg','Docker Compose: сервер, PostgreSQL и постоянные тома',body,677)

body = text(48,52,'GAME LOOP / ЖИЗНЕННЫЙ ЦИКЛ ИГРОКА',16,GREEN,700)
body += text(48,100,'Команды клиента. Правила сервера.',36,INK,700)
steps = [('01','Войти','Карта + имя','Токен игрока'),('02','Двигаться','Команды + тики','Дороги и границы'),('03','Собрать','Подбор предметов','Сумка игрока'),('04','Доставить','Пункт доставки','Начисление очков')]
for i,(num,title,a,b) in enumerate(steps):
    x=48+i*282
    body += rect(x,155,258,199)+text(x+22,191,num,19,GREEN,700)+text(x+22,239,title,27,INK,700)+text(x+22,280,a,18,MUTED)+text(x+22,310,b,18,MUTED)
    if i<3: body+=line(x+258,253,x+276,253,arrow=True)
body += card(48,417,1104,116,'Завершение сессии',[f"После {CONFIG['dogRetirementTime']:g} секунд бездействия результат сохраняется через Repository в PostgreSQL."],GOLD)
body += text(48,583,'Значение тайм-аута соответствует текущей конфигурации игры.',18,MUTED)
save('game-loop.svg','Игровой цикл: вход, движение, сбор, доставка и сохранение рекорда',body)
print(f'Generated {len(list(OUT.glob("*.svg")))} portfolio images.')
