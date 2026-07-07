<#
.SYNOPSIS
    Скрипт для одновременного запуска всех компонентов проекта.
.DESCRIPTION
    Запускает Backend, Симулятор, AI-Агент, UI, Visualizer и Mobile Web-версию
    в отдельных окнах PowerShell. Ожидает закрытия главного скрипта, чтобы остановить их все.
#>

$ErrorActionPreference = "Stop"

$ProjectRoot = $PSScriptRoot
$Processes = @()

function Write-Info($Message) {
    Write-Host "[INFO] $Message" -ForegroundColor Cyan
}

function Write-Success($Message) {
    Write-Host "[OK] $Message" -ForegroundColor Green
}

# Функция для безопасного запуска в новом окне
function Start-Component($Title, $Path, $Command) {
    Write-Info "Запускаем $Title..."
    $Args = "-NoProfile", "-NoExit", "-Command", "& { `$Host.UI.RawUI.WindowTitle = '$Title'; cd '$Path'; $Command }"
    $proc = Start-Process powershell -ArgumentList $Args -PassThru
    $Processes += $proc
}

try {
    # 1. Симулятор (TCP)
    Start-Component "Симулятор (Port 5055)" "$ProjectRoot\simulation-emulator" "python tcp_emulator.py --host 127.0.0.1 --port 5055 --agent-mode manual"

    # 2. Backend
    # Добавляем небольшую задержку, чтобы симулятор успел занять порт
    Start-Sleep -Seconds 1
    Start-Component "Backend (Port 8080)" "$ProjectRoot\backend" ".\gradlew.bat run"

    # 3. AI Агент
    # Ждем, чтобы Backend успел начать подниматься
    Start-Sleep -Seconds 2
    Start-Component "AI Агент" "$ProjectRoot\ai" "if (Test-Path '.venv\Scripts\activate.ps1') { .\.venv\Scripts\activate.ps1 }; python -m agent_service"

    # 4. Self-Play UI
    Start-Component "Self-Play UI (Port 5173)" "$ProjectRoot\self-play-ui" "python -m http.server 5173 --bind 0.0.0.0"

    # 5. AI Visualizer
    Start-Component "AI Visualizer (Port 5174)" "$ProjectRoot\ai-visualizer" "python -m http.server 5174 --bind 0.0.0.0"

    # 6. Mobile Web (Раздача статики)
    Start-Component "Mobile Web (Port 5175)" "$ProjectRoot\mobile\build\web" "python -m http.server 5175 --bind 0.0.0.0"

    Write-Host ""
    Write-Success "=== Все компоненты запущены в отдельных окнах ==="
    Write-Host ""
    Write-Host "Ссылки:"
    Write-Host "  - Backend API:       http://127.0.0.1:8080/api/docs"
    Write-Host "  - Self-Play UI:      http://127.0.0.1:5173"
    Write-Host "  - AI Visualizer:     http://127.0.0.1:5174"
    Write-Host "  - Mobile Web:        http://127.0.0.1:5175"
    Write-Host ""
    Write-Host "Нажмите ENTER или закройте это окно (Ctrl+C), чтобы остановить все запущенные компоненты." -ForegroundColor Yellow
    
    # Ожидание ввода от пользователя
    Read-Host
} finally {
    Write-Info "Останавливаем запущенные компоненты..."
    foreach ($proc in $Processes) {
        if (-not $proc.HasExited) {
            # Принудительно завершаем дочернее окно PowerShell и его подпроцессы
            Stop-Process -Id $proc.Id -Force -PassThru -ErrorAction SilentlyContinue | Out-Null
        }
    }
    Write-Success "Все компоненты остановлены."
}
