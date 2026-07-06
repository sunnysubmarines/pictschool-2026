# Задание: C-прослойка управления платформой

## Зачем это нужно

В проекте есть две версии платформы:

- симуляция в Webots;
- будущая реальная платформа на ESP32 с шасси и моторами.

Нам нужно, чтобы логика сложного движения писалась один раз на C и могла работать в обеих средах. Поэтому управление разделено на два слоя:

```text
backend / AI visualizer
        |
        v
simulation-emulator/webots_bridge.py
        |
        v
controllers/supervisor/movement_layer.c
        |
        v
Webots supervisor + e-puck controllers
```

`webots_bridge.py` больше не переводит команды сам. Он прокидывает коды команд в Webots. C-прослойка `movement_layer.c` решает, как школьная команда разворачивается в низкоуровневые команды платформы.

## Базовые команды

Базовые команды приходят из backend:

| Код | Значение |
| --- | --- |
| `1` | вперед на одну клетку |
| `2` | назад на одну клетку |
| `3` | повернуть влево |
| `4` | повернуть вправо |

Внутри Webots-платформы используются низкоуровневые команды:

| Константа | Значение |
| --- | --- |
| `PLATFORM_CMD_FORWARD` | вперед |
| `PLATFORM_CMD_BACKWARD` | назад |
| `PLATFORM_CMD_TURN_LEFT` | повернуть влево |
| `PLATFORM_CMD_TURN_RIGHT` | повернуть вправо |

Они объявлены в:

```text
controllers/supervisor/movement_layer.h
```

## Уже реализованные комплексные команды

Сейчас в `movement_layer.c` есть примеры:

| Код | Команда | Во что разворачивается |
| --- | --- | --- |
| `10` | вперед на две клетки | `forward`, `forward` |
| `11` | разворот | `left`, `left` |
| `12` | шаг вправо | `right`, `forward`, `left` |
| `13` | шаг влево | `left`, `forward`, `right` |

Например, команда `12` позволяет сместиться вправо относительно текущего направления и снова смотреть туда же, куда робот смотрел до маневра.

## Где писать код

Главный файл задания:

```text
computer-systems/sim/summer_school/controllers/supervisor/movement_layer.c
```

Заголовочный файл:

```text
computer-systems/sim/summer_school/controllers/supervisor/movement_layer.h
```

Вам нужно добавлять новые команды в два места:

1. В `movement_layer.h` добавить код команды:

```c
#define SCHOOL_CMD_SQUARE 20
```

2. В `movement_layer.c` описать, во что она разворачивается:

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

Также добавьте новую команду в `movement_is_supported_command()`.

## Что можно реализовать

Идеи для расширения:

- `20` — проехать квадрат и вернуться в исходное направление;
- `21` — объехать препятствие справа;
- `22` — объехать препятствие слева;
- `23` — сделать разведочный зигзаг;
- `24` — подъехать к соседней клетке и вернуться назад;
- `25` — развернуться и отъехать на одну клетку.

Важно: одна школьная команда может разворачиваться в несколько низкоуровневых команд.


## 7. Проверить напрямую через консоль без backend

Это самая быстрая проверка C-слоя.

1. Запустите Webots world и нажмите `Run`.

2. В отдельном терминале подключитесь к первому роботу:

```bash
nc localhost 10000
```

3. Отправьте новую команду:

```text
20
```

Нажмите Enter.

Ожидаемое поведение:

```text
robot проезжает квадрат:
вперед -> направо -> вперед -> направо -> вперед -> направо -> вперед -> направо
```

В консоли Webots должны появиться сообщения вроде:

```text
e-puck received commands: 20
e-puck parsed 8 commands.
e-puck sent command 1 -> cell (...)
e-puck turn, new orientation ...
```

Проверка второго робота:

```bash
nc localhost 10001
```

Затем снова:

```text
20
```

Что важно проговорить:

```text
Мы сейчас проверили только Webots + C-прослойку.
Backend и visualizer пока могут не знать про команду 20.
```



## 8. Интеграция команды в backend

Чтобы команда дошла из visualizer/backend до симуляции, backend должен ее разрешить.

Откройте:

```text
backend/src/main/kotlin/school/pict/backend/RoundEngine.kt
```

Найдите:

```kotlin
private val allowedCommands = setOf(1, 2, 3, 4, 10, 11, 12, 13)
```

Добавьте `20`:

```kotlin
private val allowedCommands = setOf(1, 2, 3, 4, 10, 11, 12, 13, 20)
```

Что сказать:

```text
Backend проверяет команды до отправки в симуляцию.
Если не добавить 20 сюда, backend вернет unknown_command.
```

## 9. Интеграция команды в Python-эмулятор результата

Сейчас backend получает JSON-ответ от симуляции через `webots_bridge.py`. Для расчета итоговой позиции bridge использует Python-логику из:

```text
simulation-emulator/tcp_emulator.py
```

Откройте файл и найдите:

```python
ALLOWED_COMMANDS = {1, 2, 3, 4, 10, 11, 12, 13}
COMMAND_EXPANSIONS = {
    1: [1],
    2: [2],
    3: [3],
    4: [4],
    10: [1, 1],
    11: [3, 3],
    12: [4, 1, 3],
    13: [3, 1, 4],
}
```

Добавьте команду:

```python
ALLOWED_COMMANDS = {1, 2, 3, 4, 10, 11, 12, 13, 20}
COMMAND_EXPANSIONS = {
    1: [1],
    2: [2],
    3: [3],
    4: [4],
    10: [1, 1],
    11: [3, 3],
    12: [4, 1, 3],
    13: [3, 1, 4],
    20: [1, 4, 1, 4, 1, 4, 1, 4],
}
```

Здесь используются backend-команды:

```text
1 — вперед
4 — поворот вправо
```

Что сказать:

```text
Webots выполняет команду через C-слой.
Backend должен получить такой же итог в своем состоянии.
Поэтому Python-эмулятор результата должен знать, что команда 20 делает квадрат.
```

## 10. Интеграция команды в AI Visualizer

Чтобы появилась кнопка:

Откройте:

```text
ai-visualizer/index.html
```

В блок кнопок добавьте:

```html
<button type="button" data-command="20">Square</button>
```

Откройте:

```text
ai-visualizer/app.js
```

В `commandLabels` добавьте:

```js
20: "SQ",
```

Пример:

```js
const commandLabels = {
  1: "F",
  2: "B",
  3: "L",
  4: "R",
  10: "F2",
  11: "U",
  12: "SR",
  13: "SL",
  20: "SQ",
};
```

Что сказать:

```text
Visualizer не знает физику движения.
Он только отправляет код команды в backend.
```

## 11. Проверить через backend и bridge

Запустите полную связку по инструкции:

```text
computer-systems/sim/summer_school/doc/STUDENTS_SETUP.md
```

Коротко:

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

Откройте:

```text
http://127.0.0.1:5174/
```

Нажмите:

```text
Connect -> New round
```

Проверка через curl:

```bash
curl -X POST http://127.0.0.1:8080/api/turn/submit \
  -H 'Content-Type: application/json' \
  -d '{"actor":"robot","commands":[20]}'
```

Ожидаемый успешный ответ:

```json
{
  "accepted": true,
  "eventId": "event-2",
  "forwardedAs": "20"
}
```

В логах bridge должно быть видно, что команда ушла в Webots:

```text
webots=20
```

В Webots должно быть видно выполнение последовательности из 8 низкоуровневых действий.

## 12. Если что-то пошло не так

### Backend вернул `unknown_command`

Проверьте:

```text
backend/src/main/kotlin/school/pict/backend/RoundEngine.kt
```

Команда `20` должна быть в `allowedCommands`.

### Webots ничего не делает

Проверьте:

1. Supervisor пересобран.
2. Webots world перезапущен после сборки.
3. В `movement_is_supported_command()` есть `SCHOOL_CMD_SQUARE`.
4. В `movement_expand_command()` есть `case SCHOOL_CMD_SQUARE`.

### Visualizer не показывает кнопку

Проверьте:

1. Кнопка добавлена в `ai-visualizer/index.html`.
2. Visualizer перезапущен или страница обновлена в браузере.
3. В `ai-visualizer/app.js` есть label для `20`.

### Backend-состояние расходится с Webots

Проверьте:

```text
simulation-emulator/tcp_emulator.py
```

Команда `20` должна быть в `ALLOWED_COMMANDS` и `COMMAND_EXPANSIONS`.


