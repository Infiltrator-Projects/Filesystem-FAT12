# Infiltrator Design consumers

This ledger tracks graphical consumers of `infiltrator-design-v1.json`.

`InfiltratorFS` is intentionally excluded from this migration while its active structural/UI work is in progress.

| Repository | UI surface | Typography state | Design ownership |
| --- | --- | --- | --- |
| MBLink | GTK / SwiftUI | Reference implementation | Product accent/identity local; shared structure through Link |
| Calendar | Cinnamon / GTK | MB Corpo roles implemented with fallback | Calendar identity local |
| Defragmenter | GTK / Cairo | MB Corpo provisioned/used; residual Cairo font alignment pending | Defragger identity local |
| RunnerScope | Tk | MB Corpo roles declared with fallback/private local discovery | RunnerScope identity local |
| Link | GTK / Win32 / SwiftUI | Shared shells; typography convergence in progress | Shared vehicle application shell |
| JAGLink | Link + SwiftUI | Link geometry; MB typography migration pending | Jaguar identity local |
| BMWLink | Link + SwiftUI | Link geometry; MB typography migration pending | BMW identity local |
| AUDILink | Link + SwiftUI | Link geometry; MB typography migration pending | Audi identity local |
| FORDLink | Link + SwiftUI | Link geometry; MB typography migration pending | Ford identity local |
| System-Monitor | GTK / Win32 | MB Corpo GTK roles; Common palette/metrics projected natively on Win32 | System Monitor identity and toolkit composition local |
| Calculator | GTK / Win32 / SwiftUI | Strict three-face MB Corpo policy; canonical families/provenance from Common | Calculator geometry and icon identity local |
| Character-Profiler | SwiftUI | System typography; migration pending | Character Profiler accent/identity local |
| Keeper-Group-Export | Tk/ttk | MB Corpo roles and graphite/silver structure applied | Keeper accent/identity local |
| WheresWally | Zabbix web widget | MB Corpo/design override applied; operational colours retained | NPS operational colour semantics local |
| Intune-Zabbix-Bridge | Zabbix web widget | MB Corpo and graphite/silver structure applied | Intune operational colour semantics local |
| ssmithnet.net | Static web / C++ generator | Common web roles with local MB Corpo assets and site-specific material treatment | Personal/site identity, layout and graphics local |
| Package Repository | Static web / C++ publisher | Common web roles; repository status semantics remain local | Repository/site identity and APT status presentation local |
| ExtFS-for-Windows | NSIS / driver tooling | Native installer chrome only; no standalone project UI theme | Platform installer behaviour local |
| Common | Library/docs | No application UI | Owns this design contract only |

The rule is simple: MB Corpo typography roles plus the shared white Day and layered Linux MBLink `#050608`/graphite/`#00ADEF` Night semantic palettes are owned by Common 1.19.10 and later; System means detect the host light/dark preference and select exactly one of those two palettes. Common also validates its ABI-bearing palette, metrics and typography records before native adapters project them. Product accents, logos, manufacturer colours, host-theme detection, page composition and domain-specific status semantics remain local. Native C/C++ consumers should prefer `include/infiltratr/design.h`; web consumers use `infiltrator-web-v1.css`. GTK/Cinnamon/SwiftUI/Win32 adapters may keep local selector/widget code, but CI should validate any generated or mirrored values against Common rather than allowing private palette truth.

SPDX-License-Identifier: GPL-3.0-or-later
