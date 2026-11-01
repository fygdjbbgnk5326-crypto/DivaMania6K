param([Parameter(Mandatory=$true)][string]$Tool,[string]$Arguments='{}')
$ErrorActionPreference='Stop'
$body=@{jsonrpc='2.0';id=1;method='tools/call';params=@{name=$Tool;arguments=($Arguments|ConvertFrom-Json)}}|ConvertTo-Json -Depth 30
$response=Invoke-WebRequest -Uri 'http://127.0.0.1:13338/mcp' -Method Post -ContentType 'application/json' -Body $body -TimeoutSec 45
$content=if($response.Content -is [byte[]]){[Text.Encoding]::UTF8.GetString($response.Content)}else{$response.Content}
$parsed=$content|ConvertFrom-Json
if($parsed.error){throw ($parsed.error|ConvertTo-Json -Depth 10)}
if($parsed.result.structuredContent){$parsed.result.structuredContent|ConvertTo-Json -Depth 30}else{$parsed.result|ConvertTo-Json -Depth 30}
