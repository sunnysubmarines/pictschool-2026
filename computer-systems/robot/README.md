# ESP32 School Robot

Arduino-проект для физической платформы ESP32. Платформа подключается к Wi-Fi, открывает TCP-сервер `5055`, принимает команды от backend и исполняет их через драйвер моторов и энкодеры.

## Что входит в проект

```text
esp32_school_robot/
  esp32_school_robot.ino   # основной скетч ESP32
  movement_layer.c         # школьная логика команд
  movement_layer.h         # коды команд и API прослойки
  COMMAND_TESTING.md       # проверка команд через nc/ncat
```

Старые тестовые скетчи не нужны.

## 1. Прошивка ESP32

1. Откройте Arduino IDE.
2. Установите поддержку ESP32 boards.
3. Откройте:

```text
computer-systems/robot/esp32_school_robot/esp32_school_robot.ino
```

4. Проверьте Wi-Fi в начале файла:

```cpp
const char* WIFI_SSID = "PIKTrobots";
const char* WIFI_PASSWORD = "PIKTrobots";
```

5. Выберите роль платформы:

Для робота:

```cpp
const char* DEVICE_ACTOR = "robot";
```

Для второй платформы-агента:

```cpp
const char* DEVICE_ACTOR = "agent";
```

6. Загрузите скетч на ESP32.
7. Откройте Serial Monitor на `115200`.

После подключения должны появиться строки:

```text
ESP32 robot backend replacement started
Wi-Fi SSID: PIKT-STUDS
TCP command port: 5055
IP address: ...
```

Запишите IP-адрес каждой ESP.

## 2. Подключение моторов

| ESP32 GPIO | Куда подключить |
| --- | --- |
| `18` | `IN1` драйвера |
| `19` | `IN2` драйвера |
| `21` | `IN3` драйвера |
| `22` | `IN4` драйвера |
| `25` | `ENA` драйвера |
| `26` | `ENB` / `EN2` драйвера |
| `32` | левый энкодер |
| `33` | правый энкодер |
| `GND` | общий минус ESP32, драйвера и питания |

Питание моторов подается на силовой вход драйвера. Не питать моторы от `3.3V` ESP32.

## 3. Проверка ESP без backend

Откройте в браузере:

```text
http://<ESP32_IP>/
```

Проверьте:

1. Кнопка `Forward` крутит моторы вперед.
2. `Back`, `Left`, `Right` работают.
3. `Stop` останавливает платформу.
4. В статусе меняются `leftTicks` и `rightTicks`, если крутить колеса.

Если HTTP-кнопки не работают, backend не виноват. Проверяйте питание, `GND`, `ENA/ENB`, `IN1..IN4`.

На этой странице также настраиваются простые движения:

| Поле | Что меняет |
| --- | --- |
| `Speed` | общий PWM моторов |
| `Forward distance` | расстояние для команды вперед |
| `Back distance` | расстояние для команды назад |
| `Ticks/cm` | калибровка энкодеров |
| `Left 90 turn ticks` | количество тиков энкодеров для поворота влево на 90 градусов |
| `Right 90 turn ticks` | количество тиков энкодеров для поворота вправо на 90 градусов |
| `Left trim` | поправка PWM левого мотора |
| `Right trim` | поправка PWM правого мотора |
| `Ramp step` | резкость разгона/торможения |
| `Ramp interval` | интервал изменения PWM |

Если поворот влево и вправо отличаются, меняйте `Left 90 turn ticks` и `Right 90 turn ticks` отдельно. Если назад едет больше/меньше, чем вперед, меняйте `Back distance` отдельно от `Forward distance`.

Все значения, введенные на HTTP-странице, сохраняются во flash-память ESP32 через `Preferences`. После перезагрузки ESP32 калибровка загружается автоматически.

Калибровка применяется на уровне низкоуровневых движений ESP32:

- `forward` использует `Forward distance`, `Ticks/cm`, `Speed`, `trim`;
- `backward` использует `Back distance`, `Ticks/cm`, `Speed`, `trim`;
- `turn left` использует `Left 90 turn ticks`, `Speed`, `trim`, PID;
- `turn right` использует `Right 90 turn ticks`, `Speed`, `trim`, PID.

Поэтому эти же настройки используются для команд из backend, AI visualizer, AI agent и комплексных команд из `movement_layer.c`.

Кнопка `Reset calibration` очищает сохраненные значения и возвращает настройки по умолчанию.

Подробная пошаговая инструкция по настройке движения:

```text
computer-systems/robot/CALIBRATION.md
```

## 4. Запуск backend

Из корня проекта:

```bash
./infrastructure/manual/start-backend.sh
```

Backend будет доступен на:

```text
http://127.0.0.1:8080
```

Если хотите сразу задать IP платформ через переменные:

```bash
SIM_ROBOT_TCP_HOST=<ROBOT_ESP_IP> \
SIM_AGENT_TCP_HOST=<AGENT_ESP_IP> \
SIM_TCP_COMMAND_PORT=5055 \
./infrastructure/manual/start-backend.sh
```

Для одного физического робота можно временно указать один и тот же IP в оба поля, но физически ESP исполнит только свою роль `DEVICE_ACTOR`.

## 5. Запуск AI agent

В новом терминале из корня проекта:

```bash
./infrastructure/manual/start-ai-agent.sh
```

Скрипт:

1. читает `ai/.env`;
2. создает `.venv`, если нужно;
3. ставит зависимости;
4. запускает `python -m agent_service`.

В `ai/.env` должны быть настройки LLM:

```env
OPENAI_API_KEY=...
OPENAI_BASE_URL=...
AGENT_LLM_MODEL=...
AGENT_BACKEND_URL=http://127.0.0.1:8080
AGENT_ACTOR_ID=agent
```

Если LLM не ответит, agent перейдет в fallback-логику и все равно выберет ход.

## 6. Запуск AI Visualizer

В третьем терминале:

```bash
./infrastructure/manual/start-ai-visualizer.sh
```

Откройте:

```text
http://127.0.0.1:5174/
```

В visualizer:

1. Нажмите `Connect`.
2. Введите `Robot ESP IP`.
3. Введите `Agent ESP IP`.
4. Нажмите `Save ESP IPs`.
5. Нажмите `Test ESPs`.

Ожидаемый результат:

```text
robot <ip>:5055 OK; agent <ip>:5055 OK
```

Если `FAIL`, backend не видит ESP по сети.

## 7. Проверка полного сценария

1. Нажмите `New round`.
2. В блоке `Robot Control` выберите команды.
3. Нажмите `Submit robot turn`.
4. В `Events` должны появиться:

```text
turn.submitted
simulation.command_sent
actor.moved
turn.completed
```

5. После этого `Active` должен стать `agent`.
6. Блок `AI Agent` должен перейти из `waiting` в `planning`, затем `submitted`.
7. В `Moves (robot + agent)` должны появиться ходы обоих участников.

Если физическая ESP временно не отвечает, можно включить `Local fallback` в visualizer. Тогда backend будет применять ход локально и сможет проверить AI/LLM без физического ответа платформы.

## 8. Команды движения

Backend отправляет школьные команды:

| Код | Действие |
| --- | --- |
| `1` | вперед |
| `2` | назад |
| `3` | поворот влево |
| `4` | поворот вправо |

ESP32 вызывает:

```c
movement_expand_command(command, platform_commands, max_commands)
```

и разворачивает школьную команду в простые платформенные команды. Сейчас слой намеренно поддерживает только базовые движения `1..4`.

Отдельная инструкция по проверке команд через `nc`/`ncat`:

```text
computer-systems/robot/esp32_school_robot/COMMAND_TESTING.md
```

## 9. Как добавлять команды

Меняются только:

```text
movement_layer.h
movement_layer.c
```

Пример добавления команды:

1. В `movement_layer.h` добавить код:

```c
#define SCHOOL_CMD_SQUARE 20
```

2. В `movement_is_supported_command()` добавить команду в список поддерживаемых.

3. В `movement_expand_command()` добавить `case`:

```c
case SCHOOL_CMD_SQUARE: {
    const int commands[] = {
        PLATFORM_CMD_FORWARD,
        PLATFORM_CMD_TURN_RIGHT,
        PLATFORM_CMD_FORWARD,
        PLATFORM_CMD_TURN_RIGHT,
        PLATFORM_CMD_FORWARD,
        PLATFORM_CMD_TURN_RIGHT,
        PLATFORM_CMD_FORWARD,
        PLATFORM_CMD_TURN_RIGHT
    };
    return write_commands(platform_commands, max_commands, commands, 8);
}
```

Если новая команда должна быть доступна в UI, ее также нужно разрешить в backend и добавить кнопку в visualizer.

## 10. Калибровка расстояния

Расстояние считается так:

```text
targetTicks = Distance * Ticks/cm
```

Если поставили `Forward distance = 5`, а робот проехал `50 см`, значит `Ticks/cm` примерно в 10 раз больше нужного.

Формула:

```text
newTicksPerCm = oldTicksPerCm * wantedCm / realCm
```

Пример:

```text
oldTicksPerCm = 20
wantedCm = 5
realCm = 50
newTicksPerCm = 20 * 5 / 50 = 2
```

Значение `Ticks/cm` можно менять на HTTP-странице ESP32.

Для настройки назад используйте `Back distance`. Для настройки поворотов используйте `Left 90 turn ticks` и `Right 90 turn ticks`: увеличивайте количество тиков, если поворот недокручивает, и уменьшайте, если перекручивает.

Калибровка сохраняется на самой ESP32. После настройки не нужно заново вводить значения перед запуском backend/AI visualizer.
