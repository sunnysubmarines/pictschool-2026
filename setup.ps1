<#
.SYNOPSIS
    Скрипт первоначальной настройки и установки проекта "Миссия: человек против ИИ-агента" для Windows.
.DESCRIPTION
    Проверяет и устанавливает необходимые зависимости (Python, Java 17, Flutter) через winget.
    Затем собирает и настраивает компоненты проекта: AI, Backend и Mobile (сборка для Web).
#>

$ErrorActionPreference = "Stop"

# Функция для вывода зеленым цветом
function Write-Success($Message) {
    Write-Host "[OK] $Message" -ForegroundColor Green
}

function Write-Info($Message) {
    Write-Host "[INFO] $Message" -ForegroundColor Cyan
}

function Write-WarningMsg($Message) {
    Write-Host "[WARNING] $Message" -ForegroundColor Yellow
}

Write-Info "Начинаем установку зависимостей..."

# Проверка наличия winget
if (!(Get-Command "winget" -ErrorAction SilentlyContinue)) {
    Write-WarningMsg "Winget не найден! Пожалуйста, обновите 'Установщик приложений' в Microsoft Store или установите зависимости вручную."
    Exit 1
}

# Проверка и установка Python
if (!(Get-Command "python" -ErrorAction SilentlyContinue) -and !(Get-Command "python3" -ErrorAction SilentlyContinue)) {
    Write-Info "Устанавливаем Python 3.12..."
    winget install -e --id Python.Python.3.12 --accept-package-agreements --accept-source-agreements
    Write-WarningMsg "Python установлен. Если он не доступен в терминале, закройте и откройте PowerShell заново и запустите скрипт еще раз."
} else {
    Write-Success "Python уже установлен."
}

# Проверка и установка Java 17
$javaInstalled = $false
if (Get-Command "java" -ErrorAction SilentlyContinue) {
    $javaVersion = java -version 2>&1 | Select-String -Pattern '"17\.'
    if ($javaVersion) {
        $javaInstalled = $true
        Write-Success "Java 17 (JDK) уже установлена."
    }
}

if (-not $javaInstalled) {
    Write-Info "Устанавливаем Java 17 (Eclipse Temurin)..."
    winget install -e --id EclipseAdoptium.Temurin.17.JDK --accept-package-agreements --accept-source-agreements
    Write-WarningMsg "Java 17 установлена. Если команда java не работает, закройте и откройте PowerShell заново."
}

# Проверка и установка Flutter
if (!(Get-Command "flutter" -ErrorAction SilentlyContinue)) {
    Write-Info "Устанавливаем Flutter SDK..."
    winget install -e --id Google.Flutter --accept-package-agreements --accept-source-agreements
    Write-WarningMsg "Flutter установлен. Возможна необходимость перезапуска терминала."
} else {
    Write-Success "Flutter уже установлен."
}

Write-Info "Установка системных зависимостей завершена."
Write-Info "--------------------------------------------"
Write-Info "Настраиваем компоненты проекта..."

$ProjectRoot = $PSScriptRoot

# 1. Настройка AI-агента
Write-Info "Настройка AI-агента..."
Push-Location "$ProjectRoot\ai"
try {
    if (-not (Test-Path ".env")) {
        Copy-Item ".env.example" ".env"
        Write-Success "Создан файл конфигурации ai/.env"
    }

    if (-not (Test-Path ".venv")) {
        Write-Info "Создаем виртуальное окружение .venv..."
        python -m venv .venv
    }
    
    Write-Info "Устанавливаем Python-зависимости..."
    # Используем путь к python внутри venv
    $VenvPython = if (Test-Path ".venv\Scripts\python.exe") { ".venv\Scripts\python.exe" } else { "python" }
    & $VenvPython -m pip install -r requirements.txt
    Write-Success "AI-агент настроен."
} finally {
    Pop-Location
}

# 2. Настройка Backend
Write-Info "Настройка Backend (загрузка Gradle-зависимостей)..."
Push-Location "$ProjectRoot\backend"
try {
    & .\gradlew.bat classes
    Write-Success "Backend настроен (зависимости скачаны)."
} catch {
    Write-WarningMsg "Сборка Backend завершилась с ошибкой. Возможно, вам требуется перезапустить терминал для обновления JAVA_HOME."
} finally {
    Pop-Location
}

# 3. Настройка Mobile (Web сборка)
Write-Info "Настройка Mobile..."
Push-Location "$ProjectRoot\mobile"
try {
    Write-Info "Скачиваем Flutter-пакеты..."
    flutter pub get
    Write-Info "Собираем Web-версию мобильного приложения..."
    flutter build web
    Write-Success "Mobile (Web) собран."
} catch {
    Write-WarningMsg "Сборка Mobile завершилась с ошибкой. Убедитесь, что flutter добавлен в PATH."
} finally {
    Pop-Location
}

Write-Info "--------------------------------------------"
Write-Success "Все компоненты успешно настроены! Теперь вы можете запустить проект с помощью start-all.ps1"
