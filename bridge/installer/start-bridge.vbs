Set sh = CreateObject("WScript.Shell")
target = Replace(WScript.ScriptFullName, "start-bridge.vbs", "start-bridge.cmd")
sh.Run """" & target & """", 0, False