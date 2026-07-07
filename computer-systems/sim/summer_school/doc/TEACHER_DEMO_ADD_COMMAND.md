# Демонстрация: добавляем новую C-команду движения

Сценарий для преподавателя на 15 минут. Цель демонстрации — показать, как в проекте появляется новая команда управления роботом: сначала в C-прослойке симуляции, затем в backend и AI Visualizer.

Пример команды:

```text
20 — проехать квадрат и вернуться в исходное направление
```

## 0. Что уже должно быть запущено

Для проверки напрямую через Webots достаточно:

1. Открыть мир:

```text
computer-systems/sim/summer_school/worlds/summer_school.wbt
```

2. Нажать `Run`.

3. Убедиться, что в консоли Webots есть:

```text
Supervisor started.
Servers on ports 10000 and 10001, waiting for connections...
```

Полный запуск backend + AI + visualizer описан здесь:

```text
computer-systems/sim/summer_school/doc/STUDENTS_SETUP.md
```

## 1. Коротко объяснить архитектуру

Покажите схему:

```text
AI Visualizer / backend
        |
        v
webots_bridge.py
        |
        v
movement_layer.c
        |
        v
supervisor.c -> e-puck controllers
```

Главная мысль:

```text
backend и visualizer знают код команды;
bridge прокидывает код в Webots;
movement_layer.c на C решает, как эта команда раскладывается на простые движения.
```

Простые низкоуровневые движения уже есть:

```text
forward
backward
turn left
turn right
```

А школьная задача — собрать из них более сложное поведение.

## 2. Открыть файлы C-прослойки

Откройте:

```text
computer-systems/sim/summer_school/controllers/supervisor/movement_layer.h
computer-systems/sim/summer_school/controllers/supervisor/movement_layer.c
```

В `movement_layer.h` покажите базовые команды:

```c
#define SCHOOL_CMD_FORWARD 1
#define SCHOOL_CMD_BACKWARD 2
#define SCHOOL_CMD_TURN_LEFT 3
#define SCHOOL_CMD_TURN_RIGHT 4
```

И уже существующие комплексные примеры:

```c
#define SCHOOL_CMD_FORWARD_TWICE 10
#define SCHOOL_CMD_U_TURN 11
#define SCHOOL_CMD_STEP_RIGHT 12
#define SCHOOL_CMD_STEP_LEFT 13
```

В `movement_layer.c` покажите идею функции:

```c
int movement_expand_command(int command, int *platform_commands, int max_commands)
```

Эта функция получает школьную команду и записывает в массив последовательность низкоуровневых команд платформы.

## 3. Добавить новую команду в `movement_layer.h`

Добавьте:

```c
#define SCHOOL_CMD_SQUARE 20
```

Рядом с другими школьными командами:

```c
#define SCHOOL_CMD_FORWARD_TWICE 10
#define SCHOOL_CMD_U_TURN 11
#define SCHOOL_CMD_STEP_RIGHT 12
#define SCHOOL_CMD_STEP_LEFT 13
#define SCHOOL_CMD_SQUARE 20
```

Что сказать:

```text
Мы договорились, что код 20 означает "проехать квадрат".
Этот код пока ничего не делает, мы только дали ему имя.
```

## 4. Добавить команду в список поддерживаемых

В `movement_layer.c` найдите:

```c
int movement_is_supported_command(int command)
```

Добавьте новую команду в условие:

```c
int movement_is_supported_command(int command) {
    return command == SCHOOL_CMD_FORWARD ||
           command == SCHOOL_CMD_BACKWARD ||
           command == SCHOOL_CMD_TURN_LEFT ||
           command == SCHOOL_CMD_TURN_RIGHT ||
           command == SCHOOL_CMD_FORWARD_TWICE ||
           command == SCHOOL_CMD_U_TURN ||
           command == SCHOOL_CMD_STEP_RIGHT ||
           command == SCHOOL_CMD_STEP_LEFT ||
           command == SCHOOL_CMD_SQUARE;
}
```

Что сказать:

```text
Это защита от неизвестных команд.
Если команду не добавить сюда, supervisor будет считать ее неподдержанной.
```

## 5. Реализовать разворачивание команды

В `movement_expand_command()` добавьте новый `case`:

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

Что объяснить построчно:

```text
PLATFORM_CMD_FORWARD     — едем на одну клетку вперед.
PLATFORM_CMD_TURN_RIGHT  — поворачиваем направо.

Повторяем это 4 раза:
1. сторона квадрата;
2. сторона квадрата;
3. сторона квадрата;
4. сторона квадрата.

Последний поворот возвращает робота в исходное направление.
```

Почему `8`:

```text
Команда "квадрат" состоит из 8 низкоуровневых действий:
4 движения вперед + 4 поворота.
```

## 6. Собрать supervisor

Из корня репозитория:

```bash
make -C computer-systems/sim/summer_school/controllers/supervisor
```

Ожидаемый результат:

```text
# compiling movement_layer.c
# linking supervisor
# copying to supervisor
```

Если Webots уже открыт, после сборки лучше полностью перезапустить мир, чтобы он точно взял новый бинарник supervisor.

В Webots можно также использовать:

```text
Build -> Build
```

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

## 13. Финальная мысль для участников

Эта демонстрация показывает полный путь команды:

```text
кнопка в visualizer
-> backend validation
-> bridge
-> C movement layer
-> Webots robot
```

В будущей реальной платформе Webots-часть заменится на ESP32, но идея останется такой же:

```text
сложная логика движения пишется в C,
а низкоуровневый слой платформы выполняет моторные действия.
```

Задача участников — придумать и реализовать новые команды, которые делают робота умнее, чем просто `вперед/назад/влево/вправо`.
