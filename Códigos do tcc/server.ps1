# Servidor Web Local para o Dashboard do Robô Cortador
$port = 8000
$url = "http://localhost:$port/"

$listener = New-Object System.Net.HttpListener
$listener.Prefixes.Add($url)

try {
    $listener.Start()
    Write-Host ""
    Write-Host "==========================================================" -ForegroundColor Cyan
    Write-Host "  Robô 4x4 Dashboard rodando em: $url" -ForegroundColor Green
    Write-Host "  Pressione Ctrl + C no terminal para encerrar o servidor." -ForegroundColor Yellow
    Write-Host "==========================================================" -ForegroundColor Cyan
    Write-Host ""

    # Abre o navegador automaticamente
    Start-Process $url

    while ($listener.IsListening) {
        $context = $listener.GetContext()
        $request = $context.Request
        $response = $context.Response

        $filePath = Join-Path $PSScriptRoot "index.html"

        if (Test-Path $filePath) {
            $buffer = [System.IO.File]::ReadAllBytes($filePath)
            $response.ContentType = "text/html; charset=utf-8"
            $response.ContentLength64 = $buffer.Length
            $response.OutputStream.Write($buffer, 0, $buffer.Length)
        } else {
            $response.StatusCode = 404
        }
        $response.Close()
    }
}
catch {
    Write-Host "Erro: $($_.Exception.Message)" -ForegroundColor Red
}
finally {
    $listener.Stop()
    $listener.Close()
}
