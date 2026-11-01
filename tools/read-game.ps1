param([UInt64]$Address=0,[int]$Length=64,[switch]$DumpChart)
$ErrorActionPreference='Stop'
if(-not ('DivaReadOnly' -as [type])) {
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class DivaReadOnly {
 [DllImport("kernel32.dll",SetLastError=true)] public static extern IntPtr OpenProcess(uint access,bool inherit,int pid);
 [DllImport("kernel32.dll",SetLastError=true)] public static extern bool ReadProcessMemory(IntPtr process,IntPtr address,byte[] data,UIntPtr size,out UIntPtr read);
 [DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr handle);
 public static byte[] Read(IntPtr handle,ulong address,int size) {
  byte[] data=new byte[size];UIntPtr n;
  if(!ReadProcessMemory(handle,new IntPtr((long)address),data,(UIntPtr)size,out n)||n.ToUInt64()!=(ulong)size) throw new Exception("ReadProcessMemory failed at "+address.ToString("X")+": "+Marshal.GetLastWin32Error());
  return data;
 }
}
'@
}
$game=Get-Process -Name DivaMegaMix | Sort-Object StartTime -Descending | Select-Object -First 1
$base=[UInt64]$game.MainModule.BaseAddress.ToInt64()
$handle=[DivaReadOnly]::OpenProcess(0x1010,$false,$game.Id)
if($handle -eq [IntPtr]::Zero){throw 'OpenProcess failed'}
try {
 if($Address) { $data=[DivaReadOnly]::Read($handle,$Address,$Length); [Convert]::ToHexString($data);return }
 'Base: 0x{0:X}' -f $base
 $getter=$base+0x266720
 $code=[DivaReadOnly]::Read($handle,$getter,32)
 'GetPVGameData candidate: '+[Convert]::ToHexString($code)
 if($code[0]-eq 0x48 -and ($code[1]-eq 0x8b -or $code[1]-eq 0x8d) -and $code[2]-eq 0x05){
  $slot=[UInt64]([Int64]$getter+7+[BitConverter]::ToInt32($code,3));$ptr=if($code[1]-eq 0x8d){$slot}else{[BitConverter]::ToUInt64([DivaReadOnly]::Read($handle,$slot,8),0)}
  'PV game pointer slot RVA: 0x{0:X}; pointer: 0x{1:X}' -f ($slot-$base),$ptr
  if($ptr){
   'PV flags: '+[Convert]::ToHexString([DivaReadOnly]::Read($handle,$ptr,8))
   'PV data vector area: '+[Convert]::ToHexString([DivaReadOnly]::Read($handle,($ptr+0xc8+0x2c4e0),64))
   $header=[DivaReadOnly]::Read($handle,($ptr+181680),24);$begin=[BitConverter]::ToUInt64($header,0);$end=[BitConverter]::ToUInt64($header,8)
   'Group count: '+(($end-$begin)/176)
   $arcade=$base+0x12f0f50
   'Arcade play flag: '+[DivaReadOnly]::Read($handle,$arcade,1)[0]
   'Arcade seconds: '+[BitConverter]::ToSingle([DivaReadOnly]::Read($handle,($arcade+78480),4),0)
   'PV clock seconds: '+[BitConverter]::ToSingle([DivaReadOnly]::Read($handle,($ptr+185148),4),0)
   if($begin -and $end -gt $begin){
    if($DumpChart){$dumpDir=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../build'));[void][IO.Directory]::CreateDirectory($dumpDir);[IO.File]::WriteAllBytes((Join-Path $dumpDir 'real-chart.bin'),[DivaReadOnly]::Read($handle,$begin,[int]($end-$begin)))}
    $groups=[DivaReadOnly]::Read($handle,$begin,[int][Math]::Min(176*6,$end-$begin))
    for($i=0;$i -lt $groups.Length;$i+=176){'Group {0}: count={1} time={2:F3}s type={3}' -f ($i/176),[BitConverter]::ToInt32($groups,$i),([BitConverter]::ToInt64($groups,$i+160)*1e-9),[BitConverter]::ToInt32($groups,$i+4)}
   }
  }
 }
}finally{[void][DivaReadOnly]::CloseHandle($handle)}
