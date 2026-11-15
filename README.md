# COMPRESSOTCHI

Compresseur FET façon 1176 / Audioscape 76A dont l'interface est un Tamagotchi argenté (écran LCD vert avec VU intégré et petit animal qui réagit à la compression).

- **VST3** Windows (x64) et macOS (Intel + Apple Silicon) — **AU** en plus sur macOS
- INPUT / OUTPUT / ATTACK / RELEASE, RATIO 4 · 8 · 12 · 20, METER GR / +8 / +4 / OFF, bouton IN (bypass)
- Les 3 boutons de l'œuf (ou les touches `A` `B` `C`) changent le thème d'écran, le personnage et le décor

## Télécharger

Les builds sont dans l'onglet **Actions** (artefacts de chaque commit) et dans **Releases** (tags `v*`).

Installation :

| OS | Dossier |
|---|---|
| Windows | `C:\Program Files\Common Files\VST3\` |
| macOS (VST3) | `/Library/Audio/Plug-Ins/VST3/` ou `~/Library/Audio/Plug-Ins/VST3/` |
| macOS (AU) | `~/Library/Audio/Plug-Ins/Components/` |

Sur macOS, un plugin téléchargé est mis en quarantaine : `xattr -cr COMPRESSOTCHI.vst3` (ou `.component`) avant de le copier.
Windows nécessite le runtime **WebView2** (déjà présent sur Windows 11 et sur la plupart des Windows 10).

## Compiler

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

JUCE 8 est téléchargé automatiquement. L'interface est la page `ui/index.html` (embarquée dans le plugin, affichée dans une WebView) ; le traitement audio est en C++ (`Source/PluginProcessor.cpp`).

## Licence

Le code JUCE est soumis à la licence JUCE (GPLv3 ou licence commerciale / plan gratuit JUCE selon l'usage) — à vérifier avant toute distribution commerciale.
