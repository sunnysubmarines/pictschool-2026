# Проверка команд и C-прослойки

Эта инструкция помогает проверить, что команды из `movement_layer.c` правильно разворачиваются и исполняются на ESP32.

## Что должно быть запущено

1. ESP32 прошита скетчем `esp32_school_robot.ino`.
2. ESP32 подключена к Wi-Fi.
3. В Serial Monitor видно:

```text
IP address: <ESP32_IP>
TCP command port: 5055
```

4. Открывается страница:

```text
http://<ESP32_IP>/
```

## Что именно проверяется

Есть два уровня команд.

Низкоуровневые платформенные команды:

| Код | Действие |
| --- | --- |
| `1` | вперед |
| `2` | назад |
| `3` | поворот влево |
| `4` | поворот вправо |

Школьные команды из `movement_layer.c`:

| Код | Разворачивается в |
| --- | --- |
| `1` | `forward` |
| `2` | `backward` |
| `3` | `turn left` |
| `4` | `turn right` |

Когда вы отправляете команду, ESP32 вызывает:

```c
movement_expand_command(...)
```

То есть так проверяется именно C-прослойка.

## Проверка через nc

На Linux/macOS:

```bash
printf "1\n" | nc <ESP32_IP> 5055
```

Ожидаемый ответ:

```text
OK
```

Проверить поворот:

```bash
printf "3\n" | nc <ESP32_IP> 5055
```

Проверить цепочку команд:

```bash
printf "1 3 1\n" | nc <ESP32_IP> 5055
```

## Проверка на Windows

В Windows `nc` обычно не установлен.

Варианты:

1. Использовать Git Bash, если установлен Git for Windows.
2. Установить Nmap и использовать `ncat`.
3. Использовать WSL.

Через Git Bash или WSL:

```bash
printf "1\n" | nc <ESP32_IP> 5055
```

Через Nmap `ncat` в PowerShell:

```powershell
"1" | ncat <ESP32_IP> 5055
```

Ожидаемый ответ:

```text
OK
```

## Что смотреть в Serial Monitor

После отправки команды должны появиться строки вида:

```text
TCP request bytes: 1
TCP payload: 1
Accepted legacy 1 school commands, expanded to 1 platform commands
Starting platform command 1 (forward)
```

Если вместо `OK` приходит:

```text
BUSY
```

значит ESP32 еще выполняет предыдущую команду. Подождите завершения или нажмите `Stop` на HTTP-странице.

Если приходит:

```text
ERR unknown_command
```

значит код команды не добавлен в `movement_layer.h` / `movement_layer.c`.

## Как калибровка влияет на команды

Калибровка, введенная на HTTP-странице ESP32, сохраняется на самой ESP32 и применяется к низкоуровневым движениям:

- `forward` использует `Forward distance`, `Ticks/cm`, `Speed`, `trim`;
- `backward` использует `Back distance`, `Ticks/cm`, `Speed`, `trim`;
- `turn left` использует `Left 90 turn ticks`, `Speed`, `trim`, PID;
- `turn right` использует `Right 90 turn ticks`, `Speed`, `trim`, PID.

Команды из `movement_layer.c` используют эти низкоуровневые движения. Поэтому если вы настроили `Forward distance` или `Left 90 turn ticks` в web UI, эти настройки будут использоваться и для:

- команд из `nc`;
- команд из backend;
- команд из AI visualizer;
- команд, которые выбрал AI agent.

## Быстрый чек-лист

1. Открыть `http://<ESP32_IP>/`.
2. Настроить `Forward distance`, `Back distance`, `Left 90 turn ticks`, `Right 90 turn ticks`.
3. Нажать `Forward`, `Back`, `Left`, `Right` на странице.
4. Проверить одиночные команды через `nc`: `1`, `2`, `3`, `4`.
5. Проверить цепочку простых команд через `nc`, например `1 3 1`.
6. Проверить, что после перезагрузки ESP32 значения на странице остались прежними.
7. Запустить backend/AI visualizer и проверить, что команды используют ту же калибровку.
