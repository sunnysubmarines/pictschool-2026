# Запуск Webots + backend + AI + visualizer

Эта инструкция поднимает всю систему вместе:

```text
Webots simulation :10000/:10001
        ^
        |
webots_bridge.py :5055
        ^
        |
backend :8080
        ^
        |
AI visualizer :5174 + AI agent
```

## Что должно быть установлено

1. Python 3.
2. Java/JDK для backend.
3. Webots R2025a.


## Подготовка AI env

Из корня проекта:

```bash
cd ai
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

Файл `ai/.env` должен содержать настройки LLM и backend. 



## Шаг 1. Запустить Webots

Откройте мир `computer-systems/sim/summer_school/worlds/summer_school.wbt`.

### Linux

Если Webots установлен из `.deb`-пакета в стандартное место, из корня проекта:

```bash
/usr/local/webots/webots computer-systems/sim/summer_school/worlds/summer_school.wbt
```

Если команда `webots` есть в `PATH`, можно короче:

```bash
webots computer-systems/sim/summer_school/worlds/summer_school.wbt
```

### macOS

Из корня проекта:

```bash
/Applications/Webots.app/Contents/MacOS/webots computer-systems/sim/summer_school/worlds/summer_school.wbt
```

Или откройте Webots из Applications, затем в меню выберите:

```text
File -> Open World...
```

и укажите файл:

```text
computer-systems/sim/summer_school/worlds/summer_school.wbt
```

### Windows



Если путь не сработал, откройте Webots через меню Windows, затем:

```text
File -> Open World...
```

и выберите файл:

```text
computer-systems\sim\summer_school\worlds\summer_school.wbt
```

После открытия мира в Webots нажмите кнопку `Run`.

В консоли Webots должны появиться строки:

```text
Supervisor started.
Servers on ports 10000 and 10001, waiting for connections...
```

Эти два порта открывает Webots:

- `10000` — robot;
- `10001` — agent.

Backend напрямую к ним не подключается. Между backend и Webots работает bridge.

## Шаг 2. Запустить backend + bridge + AI agent

Откройте новый терминал в корне проекта:

```bash
SIM_DRIVER=webots ./infrastructure/manual/start-backend-ai-stack.sh
```

Этот скрипт делает сразу три вещи:

1. Запускает `simulation-emulator/webots_bridge.py` на `127.0.0.1:5055`.
2. Запускает backend на `http://127.0.0.1:8080`.
3. Запускает AI agent из папки `ai`.

Ожидаемые строки:

```text
[stack] starting Webots bridge on 127.0.0.1:5055 -> 127.0.0.1:10000/10001
[stack] starting backend on http://127.0.0.1:8080
[stack] backend + simulation + agent are running
```

Оставьте этот терминал открытым.

## Шаг 3. Запустить AI visualizer

Откройте еще один терминал в корне проекта:

```bash
./infrastructure/manual/start-ai-visualizer.sh
```

Откройте в браузере:

```text
http://127.0.0.1:5174/
```

В поле backend URL должно быть:

```text
http://127.0.0.1:8080
```

Нажмите:

1. `Connect`
2. `New round`

После `New round` bridge отправит Webots начальную расстановку поля:

```text
[backend-sync] webots=SETUP round-1 10 10 ...
```

В консоли Webots должно появиться:

```text
received setup: SETUP ...
Applied backend setup to Webots scene.
```

## Как играть

В AI Visualizer:

1. Вы ходите за `robot`.
2. Наберите до 5 команд.
3. Нажмите `Submit robot turn`.
4. После хода robot AI agent автоматически сделает ход за `agent`.

Базовые команды:

| Код | Кнопка | Действие |
| --- | --- | --- |
| `1` | Forward | вперед |
| `2` | Back | назад |
| `3` | Left | поворот влево |
| `4` | Right | поворот вправо |

Расширенные команды из C-прослойки:

| Код | Кнопка | Действие |
| --- | --- | --- |
| `10` | Forward x2 | две клетки вперед |
| `11` | U-turn | разворот |
| `12` | Step right | шаг вправо |
| `13` | Step left | шаг влево |

## Что делает bridge

Backend говорит с симуляцией только через один порт:

```text
backend -> 127.0.0.1:5055
```

`webots_bridge.py` принимает JSON от backend и отправляет команды в Webots:

```text
robot -> Webots :10000
agent -> Webots :10001
```

Начальная расстановка тоже идет через bridge. При каждом `New round` visualizer вызывает backend, backend пишет событие `round.started`, bridge видит это событие и заново отправляет `SETUP` в Webots. Поэтому утки, роботы и платформы должны сбрасываться без ручного перезапуска Webots.

## Если нужно запустить bridge отдельно

Обычно это не нужно, потому что `start-backend-ai-stack.sh` запускает bridge сам.

Но для отладки можно запустить отдельно:

```bash
SIM_DRIVER=webots \
WEBOTS_BACKEND_URL=http://127.0.0.1:8080 \
AGENT_MODE=manual \
./infrastructure/manual/start-simulation.sh
```

Тогда backend нужно запустить в другом терминале:

```bash
./infrastructure/manual/start-backend.sh
```

И AI agent отдельно из папки `ai`:

```bash
cd ai
source .venv/bin/activate
python -m agent_service
```

## Частые проблемы

### `Address already in use` на порту 5055

Значит старый bridge или emulator еще работает.

Проверить:

```bash
lsof -nP -iTCP:5055 -sTCP:LISTEN
```

Остановить процесс:

```bash
kill <PID>
```

### Backend не может отправить ход в симуляцию

Проверьте:

1. Webots запущен и пишет `Servers on ports 10000 and 10001`.
2. Стек запущен с `SIM_DRIVER=webots`.
3. Bridge слушает `127.0.0.1:5055`.
4. Порт `5055` не занят старым процессом.

### После `New round` Webots не обновляет поле

Проверьте, что bridge запущен с backend URL. В полном стеке это делается автоматически:

```bash
SIM_DRIVER=webots ./infrastructure/manual/start-backend-ai-stack.sh
```

В логах bridge должна быть строка:

```text
backend_url=http://127.0.0.1:8080
```

### LLM возвращает 404 или другую ошибку

Игра все равно может работать через fallback-логику агента.

Для проверки всей связки без LLM:

```bash
AGENT_LLM_ENABLED=false SIM_DRIVER=webots ./infrastructure/manual/start-backend-ai-stack.sh
```

Если нужен LLM, проверьте в `ai/.env`:

```env
OPENAI_API_KEY=...
OPENAI_BASE_URL=...
AGENT_LLM_MODEL=...
```

`OPENAI_BASE_URL` должен быть совместим с OpenAI Chat Completions API.

### Роботы двигаются не теми командами

Пересоберите контроллеры и перезапустите Webots:

```bash
make -C computer-systems/sim/summer_school/controllers/epuck_controller
make -C computer-systems/sim/summer_school/controllers/supervisor
```

После сборки полностью перезапустите Webots world.

## Быстрый happy path

Терминал 1:

```bash
/usr/local/webots/webots computer-systems/sim/summer_school/worlds/summer_school.wbt
```

В Webots нажать `Run`.

Терминал 2:

```bash
SIM_DRIVER=webots ./infrastructure/manual/start-backend-ai-stack.sh
```

Терминал 3:

```bash
./infrastructure/manual/start-ai-visualizer.sh
```

Браузер:

```text
http://127.0.0.1:5174/
```

Нажать `Connect`, затем `New round`.
