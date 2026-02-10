# TI

## Automation Quick Start

### One-command build + checks
```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\auto_build_and_check.ps1 -Config Debug -Reconfigure
```

### Full sweep (edge cases, resources, crash reporter)
```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\auto_build_and_check.ps1 -Config Debug -RunEdgeCases -CheckResources -CheckCrashReporter
```

### Full sweep + JSON analysis report
```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\auto_build_and_check.ps1 -Config Debug -RunEdgeCases -CheckResources -RunAnalysis
```

### Nightly mode (full checks + archived artifacts)
```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\auto_build_and_check.ps1 -Config Debug -Nightly
```

Optional nightly controls:
`-NightlyRoot artifacts -NightlyKeep 14`

### Event automation only
```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run_event_automation.ps1 -Config Debug -Scenario baseline
```

### Analyze latest logs
```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\analyze_logs.ps1
```

### Run edge-case scenarios only
```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\test_edge_cases.ps1 -Config Debug
```

### Resource usage check
```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\check_resources.ps1 -Config Debug
```

### Crash reporter smoke test
```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\check_crash_reporter.ps1 -Config Debug
```
