# Top Vocal Chain + Top Tune + Top Summing

Este repositorio contiene **veinticuatro plugins** (VST3 para Windows y macOS, + AU en macOS): **Top Vocal Chain**, **Top Vocal Chain V2**, **Top Vocal Chain V3**, **Top Tune**, **Top Summing**, **Top Strip 737**, **Top Strip 737 V2**, **Top Strip 737 V3**, **Top Drums**, **Top Blue Comp**, **Top Choir**, **Top Voice MIDI**, **Top Note Editor**, **Top Smooth**, **Top Master**, **Top Rack**, **Top Console B**, **Top Meter**, **Top Mini CH**, **Top Mic Collection**, **Top Locker**, **Top EQ-1A**, **Top 73** y **Top VoxStrips** (antes 6961P).

---

## Top Vocal Chain

Plugin **VST3** (Windows y macOS, + AU en macOS) con una cadena vocal completa en un solo plugin:

**Autotune → Preamp → EQ → Compresor → Wider → Delay → Reverb**, más una perilla **IA Voice** que escucha tu voz y ajusta la cadena sola.

Interfaz estilo tira de consola (diseño original), escalable de 100 % a 200 % y con módulos que se pueden ocultar: módulos verticales lado a lado, perillas con tapa de color por función, botones IN iluminados, pantallas ámbar, curva del EQ en vivo, medidores LED de entrada/salida/GR y fader de salida. Doble clic en una perilla la regresa a su valor por defecto; al girarla se muestra su valor exacto.

## Módulos

| Módulo | Controles |
|---|---|
| **Input / Output** | Faders con vúmetro |
| **Autotune** | Motor natural que conserva los formantes. Tonalidad + escala (Cromática, Mayor, Menor natural, Menor armónica, Pentatónicas, Blues) con **AUTO KEY** (detecta la tonalidad cantando). Modos **NATURAL** y **ROBOTIC**. Retune Speed, Amount, **Humanize** (notas largas más naturales), **Flex-Tune** (solo afina al acercarse a la nota), **Formant** (±5 st), **Transpose** (±12 st), **Vibrato**, **Tracking** y **LOW LAT** (~14 ms para grabar). Pantalla con nota objetivo, nota cantada, Hz, desviación en cents, indicador de voz y de afinación |
| **IA Voice** | Perilla *Inteligencia* (0–100 %), *Escuchar*, *Reset*. Botón **ANALIZAR VOZ**: cantas ~8 s y abre un informe (registro vocal Bajo/Barítono/Tenor/Contralto/Mezzosoprano/Soprano, frecuencia media, tonalidad, nivel, dinámica, resonancias, sibilancia, presencia, graves, ruido) con el botón **APLICAR RECOMENDACIONES** |
| **Preamp** | 8 modelos: **Clean** (transparente), **Tube** (armónicos cálidos), **Vintage** (saturación suave), **Modern** (brillante y limpio), **Tape** (compresión de cinta), **Transformer** (color de hierro), **Transistor**, **Diode**. Drive, Warmth, Trim, Input. Oversampling 2x |
| **EQ** | 4 modelos (Limpio, estilo Neve 1073, API 550/5500, Pultec EQP-1A). **Gráfica grande** con la curva real en vivo, **puntos arrastrables** (rueda del mouse en MID = Q, doble clic = reset) y valores al pasar el mouse. Low Cut y **High Cut** con pendientes de 6/12/18/24 dB/oct, Low, Mid (frecuencia, ganancia, Q), Presence, Air |
| **Compresor** | 7 modos: **Clean VCA**, **Vintage Opto**, **FET Punch**, **Vari-Mu Glue**, **LA Vocal**, **Fast**, **Smooth**. Threshold, Ratio, Attack, Release, **Knee**, Makeup, **Mix** (paralela), **filtro HPF del detector** y **sidechain externo**. **Curva de compresión** en vivo y medidores IN / GR / OUT |
| **Stereo Lab** | 5 efectos (**Wide**, **Chorus**, **Double**, **Shift**, **Dimension**) con perilla Efecto, **Width 0–200 %**, **Mix**, botón **MONO**, modo **SAFE VOCAL** (graves siempre en mono y **corrección automática de fase**: si la correlación se vuelve negativa, cierra el ancho solo y lo devuelve cuando la fase está bien; la pantalla muestra "AUTO FASE -x%") y medidor de **correlación** con aviso de **problema de fase** |
| **Delay / Reverb** | Un módulo con dos pestañas. **Delay**: Time, Feedback, Mix, Width, Lo Cut, Hi Cut, Saturation, Modulation, Ducking, Sync (1/2 … 1/16, con puntillo y tresillo), Ping Pong. **Reverb**: tipos **Room, Plate, Hall, Chamber, Vocal Plate**, Pre-delay, Decay, Size, Damping, Early Reflections, Diffusion, Lo Cut, Hi Cut, Ducking, Width, Mix |
| **Meters** | Peak hold, **True Peak**, RMS, **LUFS momentáneo e integrado**, aviso de clip, correlación y medidores IN/OUT/GR. Clic en la pantalla para resetear |

Cada módulo tiene su botón **IN** (verde = activo, gris = apagado, naranja = saturando). Los módulos se pueden ocultar desde **MÓDULOS** y la ventana tiene tamaños 100/125/150/200 %.

### Flujo de trabajo (barra superior y módulo OUTPUT)

| Función | Qué hace |
|---|---|
| **Tamaño 100 / 125 / 150 / 200 %** | Agranda toda la interfaz sin perder nitidez |
| **MÓDULOS** | Oculta los módulos que no usas; el resto se reacomoda y la ventana se achica |
| **IN de cada módulo** | Verde = activo, gris = apagado (el módulo se ve atenuado), naranja = saturando |
| **Bypass sin clicks** | Encender/apagar cualquier módulo hace un fundido de 20 ms |
| **UNDO / REDO / RESET** | Deshacer, rehacer y volver a los ajustes por defecto |
| **A / B / COPIAR** | Dos juegos de ajustes para comparar cadenas; COPIAR pasa el actual a la otra ranura |
| **MANTENER: ORIGINAL** | Mientras lo mantienes pulsado escuchas la voz original **al mismo volumen** que la procesada |
| **BYPASS** | Bypass general de toda la cadena (sin clicks y alineado con la latencia) |
| **DRY / WET** | Mezcla general entre la voz original y la procesada |
| **LIMIT -0.3 dB** | Limitador de salida opcional |
| **Medidores** | Clic en un medidor para resetear el pico; CLIP se enciende en rojo si la salida satura |

### Modelos analógicos

**Saturación (Preamp)**
- **Clean:** solo ganancia, transparente.
- **Tube:** cálida, armónicos pares e impares. *Warmth* = más asimetría (más pares).
- **Vintage:** saturación suave, cuerpo en medios-graves y agudos redondeados.
- **Modern:** brillante y limpio, graves firmes.
- **Tape:** comprime suave y redondea los agudos como una cinta. *Warmth* = más oscura.
- **Transformer:** satura sobre todo los graves ("hierro"); da cuerpo sin ensuciar.
- **Transistor:** armónicos impares, más filo y mordida.
- **Diode:** asimétrico y agresivo, para efectos y rap duro.

**Compresor**
- **Clean VCA:** transparente, respeta Attack y Release tal cual.
- **LA Vocal:** opto más rápido y denso, deja la voz al frente.
- **Fast:** ataque y release rapidísimos para controlar picos.
- **Smooth:** detección lenta, compresión muy suave.
- **Opto (estilo LA-2A):** lento y musical; cuanto más comprime, más lento suelta. Color de válvula suave.
- **FET (estilo 1176):** atrapa los picos en ~1 ms, agresivo y con presencia. Attack se vuelve 10 veces más rápido.
- **Vari-Mu (estilo Fairchild):** el ratio sube con el nivel; "pega" la voz de forma muy suave.

**EQ**
- **Limpio (Digital):** preciso, sin color.
- **Británico (estilo Neve 1073):** frecuencias por pasos como el original (Mid: 360, 700, 1.6k, 3.2k, 4.8k Hz; Low Cut: 50/80/160/300 Hz a 18 dB/oct), curvas anchas y color de transformador.
- **Americano (estilo API 550/5500):** "Q proporcional": cuanto más subes, más estrecha la curva. Mid por pasos (400, 800, 1.5k, 3k, 5k Hz). Punch y color.
- **Pasivo (estilo Pultec EQP-1A):** al subir **Low** hace el famoso "truco Pultec" (realce de graves + corte encima, graves grandes sin barro). Presencia amplia en 8 kHz, aire sedoso y color de válvula.

> Son modelos inspirados en el carácter de esos equipos; no son emulaciones oficiales ni están afiliados a sus fabricantes.

### Novedades: visuales premium, más procesamiento y funciones de estudio

**Controles y pantalla**
- Perillas con aro metálico, anillo LED que sigue el valor (desde el centro en las bipolares) y el valor exacto mientras las tocas.
- **Doble clic** en una perilla = escribir el valor exacto · **Shift** al arrastrar = ajuste fino · rueda del mouse = ajuste fino · **Alt+clic** = valor por defecto.
- Tooltips profesionales en todos los controles.
- **Analizador de espectro** en el EQ (POST o PRE + POST).
- **Gráfico de afinación en vivo** en el Autotune: la voz que entra (ámbar) y la corregida (verde) sobre las notas de la escala.
- **Imagen estéreo** semicircular en Stereo Lab con medidor de ancho, **MONO CHECK** y aviso de compatibilidad mono.

**Procesamiento**
- Compresor: nuevos modos **Tube** y **Modern Digital** (auto-release), zona de knee en la curva y gráfico de reducción de ganancia en el tiempo (se ve el Attack y el Release).
- Preamp: botón **ANALOG** (tolerancias de componentes y ruido de fondo muy bajo) e indicador **SAT** verde / amarillo / rojo.
- EQ: banda MID **Bell / Notch**, modo **Stereo / Mid / Side**, **SOLO MID** y **EQ dinámico** (DYNAMIC + DYN THR).
- Delay: tipos **Digital, Tape, Analog, Slap**, **TAP tempo** y gráfico de las repeticiones.
- Reverb: tipos nuevos **Vintage** y **Modern** y gráfico de reflexiones y cola.

**Funciones de estudio**
- **PRESET BROWSER**: presets por género, artista, tipo de voz y estilo de mezcla, más tus presets guardados.
- **MIDI Learn**: clic derecho en una perilla → MIDI Learn → mueve un control de tu teclado MIDI (se guarda con el proyecto).
- **OPCIONES**: calidad **Eco / Normal / High** (oversampling 1x / 2x / 4x), tema **Hardware / Oscuro**, quitar MIDI, **exportar / importar configuración** (.tvcconfig).
- Indicador de **latencia** y calidad en la barra superior.

### VOCAL AIR (aire sedoso y riqueza armónica)
Módulo después del compresor, con diseño propio:
- **MID AIR**: presencia aireada alrededor de 5 kHz (hasta +8 dB).
- **HIGH AIR**: aire arriba del todo, centrado por defecto en **20 kHz**, con una curva muy suave (6 dB/oct) que sube poco a poco desde ~8 kHz: abre la voz sin hacerla chillona (hasta +12 dB).
- **AIR FREQ**: dónde se centra el aire (8–20 kHz).
- **HARMONICS**: genera armónicos **2º y 3º reales** de la zona 2.5–7 kHz que rellenan 5–20 kHz (polinomios de Chebyshev: solo esos armónicos, sin aliasing). Da riqueza armónica, no solo brillo.
- **SILK**: más armónico par (cálido y sedoso) y redondea los picos del aire.
- **SMOOTH**: baja el aire automáticamente en las "S" para que no pique.
- **TRIM**: volumen de salida del módulo.

## Presets

Incluye 18 presets de fábrica: *Default, Dominican Dembow, Trap Aggressive, Modern Reggaeton, Bad Bunny Style, El Alfa Style, Melodic Trap, Clean Pop Vocal, R&B Smooth, Rap Punch, Drill Vocal, Bachata Modern, Studio Clean, Clásico Neve + 1176, Pultec + LA-2A (Suave), API + Cinta (Punch), Lo-Fi, Podcast / Voz Hablada*.

> Los presets "Style" son ajustes inspirados en el sonido general de esos géneros/artistas. No están afiliados ni aprobados por ellos.
Con **GUARDAR** / **CARGAR** guardas tus propios presets (`.tvcpreset`) en `Documentos/Top Vocal Chain/Presets`.

## Cómo funciona la perilla IA Voice

Es un motor de **análisis adaptativo en tiempo real** (no una red neuronal). Mientras cantas mide:

- nivel de la toma → ajusta la ganancia de entrada (gain staging a ~-20 dBFS RMS)
- exceso de graves → sube el Low Cut
- "barro" 200–500 Hz → recorta en 330 Hz
- falta o exceso de brillo → ajusta Presencia y Aire
- sibilancia 5–9 kHz → de-esser en 7 kHz
- rango dinámico → baja el threshold y sube el ratio del compresor
- qué tan desafinado cantas → acelera el autotune

La perilla decide **cuánto** de esas correcciones se aplica (0 % = solo tus ajustes, 100 % = corrección completa). Ignora los silencios. Apaga *Escuchar* para congelar el análisis; *Reiniciar* lo empieza de cero.

## Top Tune

Afinador vocal en tiempo real con cambio de tono y de formantes.

| Función | Qué hace |
|---|---|
| **Corrección en tiempo real** | Afina mientras grabas. Tonalidad + escala (Cromática, Mayor, Menor, Menor Armónica, Pentatónicas, Blues, Personalizada) |
| **Estilos naturales** | *Natural* (respeta vibrato y deslizamientos), *Expresivo* (melodías rápidas), *Sostenido* (solo afina las notas largas) |
| **Velocidad / Cantidad** | Velocidad 0 ms = efecto duro tipo trap; valores altos = corrección suave |
| **Pitch Shift + Fino** | ±12 semitonos y ±100 cents, conservando los formantes (sin voz de "ardilla"). Con **Mix** al 50 % creas armonías |
| **Formant** | ±5 semitonos: cambia el carácter de la voz (más grave/más aguda) sin cambiar la nota |
| **MIDI Repitch** | Toca una nota en tu teclado MIDI y la voz canta esa nota |
| **MIDI Escala** | Toca las notas de tu escala en el teclado MIDI y la voz queda fija a esas notas |
| **Detector de tonalidad** | Pulsa *Detectar tonalidad*, canta o reproduce la pista unos segundos y pulsa *Aplicar* |
| **Teclado de escala** | Haz clic en las teclas para crear tu propia escala |
| **Modo Live** | Latencia de ~6 ms para monitorear mientras grabas (sin control de formantes). Modo normal: ~36 ms, compensados por el DAW |
| **Gráfica de tono** | Gris = tu voz, verde = salida afinada |

**Sidechain del compresor en FL Studio:** en el Mixer, envía la pista que debe controlar la compresión a la pista de la voz con *Sidechain to this track*; luego activa **SIDECHAIN EXT** en el compresor.

**MIDI en FL Studio:** en el plugin abre el menú de engranaje (*Wrapper settings*), en **MIDI** pon un *Input port* (por ejemplo 1) y en tu teclado/canal MIDI envía al mismo puerto.

### Motor nuevo (v2)
- **3 motores** (abajo a la derecha):
  - **REALTIME** (~7 ms): remuestreo con saltos de ciclo completo, sin voz doble.
  - **LOW LAT** (~29 ms): PSOLA con formantes.
  - **HIGH Q** (~43 ms): PSOLA con marcas de tono alineadas por correlación.
- Seguimiento más estable: el detector estima el tono el doble de seguido, con histéresis de voz y protección contra saltos de octava.
- Interfaz negro / plata (tema plata opcional):
  - panel KEY & SCALE con afinador ±50 cents y botón AUTO;
  - piano para armar la escala;
  - SUSTAIN, STRENGTH, SPEED, FORMANT, LINK, SHIFT, FINE, MIX y OUTPUT;
  - ENGINE, undo/redo y ESTILO.

## Top Summing

Sumador analógico estilo consola británica (inspirado en el Neve 8816). Da el "pegamento", peso y profundidad de mezclar por una consola.

**Cómo se usa:** pon una instancia en **modo CHANNEL** en cada pista (o en los grupos) y una en **modo BUS** en el master. Cada instancia es como un canal distinto de la consola.

| Control | Qué hace |
|---|---|
| **DRIVE** | Cuánto empujas la señal hacia el transformador (-6 a +18 dB). Más drive = más densidad y armónicos |
| **IRON** | Color del transformador: satura más los graves que los agudos y da "peso" |
| **BUS / CHANNEL** | Switch de modo. BUS agrega crosstalk entre canales y un poco más de color |
| **WIDTH** | Ancho estéreo (Mid/Side): 0 = mono, 100 = normal, 200 = extra ancho |
| **CROSSTALK** | Cuánto se "escuchan" los canales entre sí, como en una consola real (solo en BUS, ~-40 dB) |
| **ANALOG** | Tolerancias de componentes: izquierda y derecha suenan levemente distintos (más profundidad) |
| **NUEVA UNIDAD** | Cambia esas tolerancias, como usar otra consola física. Se guarda con tu proyecto |
| **NOISE** | Ruido de fondo analógico (~-96 dBFS) |
| **SUM IN** | Encendido / bypass |
| **OUTPUT + VU** | Nivel de salida con VU meter de aguja |

Incluye 9 presets: *Neutral (canal), Canal Cálido, Canal Empujado (Hot), Bus de Mezcla, Bus Pegado (Glue), Bus de Batería, Bus de Voces, Master Abierto, Vintage con Ruido*.

> Modelo inspirado en el carácter de esas consolas; no es una emulación oficial ni está afiliado a su fabricante.

## Descargar los plugins compilados

Cada vez que se sube código, GitHub Actions compila para Windows y macOS automáticamente:

1. Ve a la pestaña **Actions** del repositorio → abre la última ejecución verde.
2. Descarga **TopVocal-Plugins-Windows** o **TopVocal-Plugins-macOS** (sección *Artifacts*). Cada zip trae todos los plugins.

Para crear una *Release* con descargas permanentes: crea un tag que empiece por `v` (por ejemplo `v1.0.0`).

## Instalación

**Windows (FL Studio, Ableton, Reaper, Cubase…)**
Copia `Top Vocal Chain.vst3`, `Top Tune.vst3` y `Top Summing.vst3` a `C:\Program Files\Common Files\VST3\`.
En FL Studio: *Options → Manage plugins → Find plugins*, y luego búscalo como **Top Vocal Chain**.

**macOS**
Copia `Top Vocal Chain.vst3` a `~/Library/Audio/Plug-Ins/VST3/` y `Top Vocal Chain.component` (AU, para Logic) a `~/Library/Audio/Plug-Ins/Components/`.
Como no está firmado con certificado de Apple, la primera vez ejecuta en Terminal:

```bash
xattr -cr ~/Library/Audio/Plug-Ins/VST3/"Top Vocal Chain.vst3"
xattr -cr ~/Library/Audio/Plug-Ins/Components/"Top Vocal Chain.component"
xattr -cr ~/Library/Audio/Plug-Ins/VST3/"Top Tune.vst3"
xattr -cr ~/Library/Audio/Plug-Ins/Components/"Top Tune.component"
xattr -cr ~/Library/Audio/Plug-Ins/VST3/"Top Summing.vst3"
xattr -cr ~/Library/Audio/Plug-Ins/Components/"Top Summing.component"
```

## Compilar tú mismo

Requisitos: CMake 3.22+, y Visual Studio 2022 (Windows) o Xcode (macOS). JUCE se descarga solo.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Prueba offline del audio (opcional): `cmake -B build -DTVC_BUILD_TESTS=ON` y ejecuta `TVCOfflineTest`.

## Datos técnicos

- Latencia: ~36 ms con el autotune natural, ~14 ms con LOW LAT (reportada al DAW, que la compensa automáticamente)
- CPU: ~2 % de un núcleo a 44.1 kHz
- Autotune: detección de tono YIN + TD-PSOLA (conserva formantes); precisión medida de ±2 cents
- Limitador suave de seguridad en la salida (nunca pasa de 0 dBFS)
- Hecho con [JUCE](https://juce.com) 8 (licencia AGPLv3 / comercial de JUCE)

---

## Top Strip 737

Channel strip de válvulas clase A (VST3 Windows/macOS + AU en macOS) modelado a nivel de circuito y **calibrado con las especificaciones publicadas del canal original** (hoja de datos y manual de operación). Panel foto-realista con la misma distribución del aparato, dibujado con el renderizador 3D propio.

| Sección | Qué hace (y con qué dato del hardware se calibró) |
|---|---|
| **Preamp (2 × 6922)** | PREAMP GAIN (escala LINE -28 a +9 dB), INPUT **LINE / INSTRUMENT / MIC** (MIC = trafo de entrada: más cuerpo en graves a nivel alto; INSTRUMENT = Hi-Z, más pares), **HIGH GAIN** (+8 dB en LINE, +18 dB en MIC/INST, menos realimentación), **PHASE REV**, **+48v** (decorativo). THD 0.025 % a +4 dBu, **0.5 % justo antes del recorte**, salida máxima **+30 dBu**, respuesta plana 10 Hz–20 kHz (±0.1 dB) |
| **High Pass** | 30–140 Hz, 6 dB/oct (**FILTER IN**) |
| **Compresor óptico** | THRESHOLD -30 a +20 dB (0 = 0 VU = -18 dBFS), COMPRESSION 1:1–20:1 (codo que se estrecha con la ratio), ATTACK 2–200 ms (más rápido con transitorios fuertes), RELEASE 100 ms–5 s en dos etapas **con memoria de la celda** (frases largas sueltan más despacio), triodo de ganancia. **EQ ▸ COMP PRE**, **METER** (VU en reducción de ganancia), sidechain enlazado en estéreo |
| **EQ discreto** | BASS shelf pasivo 15/30/60/150 Hz ±24 dB, LOW MID 35–450 Hz (x10: 350 Hz–4.5 kHz) ±16 dB, HIGH MID 220 Hz–2.8 kHz (x10: 2.2–28 kHz) ±16 dB, **HI-Q** (Q 0.2 → 0.85), TREBLE shelf pasivo 10/15/20/32 kHz ±20 dB, **SC ◂ MIDS** (las bandas medias solo en el sidechain = de-esser), **EQUALIZER IN** |
| **Salida** | OUTPUT -40 a +10 dB, VU de aguja (0 VU = +4 dBu), interruptor **POWER** (apagado = señal original, sin clicks) |

Calidad: todo el camino no lineal corre a **8x con filtros de -110 dB, en doble precisión y con anti-aliasing por antiderivada (ADAA)** en cada etapa de válvula: incluso con 13 dB de recorte a 7 kHz el aliasing queda por debajo de -94 dB. Las curvas del EQ coinciden con la función de transferencia del circuito con un error < 0.1 dB. Las perillas usan la misma escala impresa en el panel (el indicador apunta al número real).

Presets: *Default, Voz Calida, Voz Rap Presencia, Voz Aire (Pop), De-Esser (SC Mids), Bass DI, Guitarra Acustica, Color de Valvula, Valvula Caliente (High Gain), Bombo Peso Sub*.

Las pruebas `TopStripTest` comprueban cada dato de la especificación (THD, salida máxima, ganancias, HPF, curvas del EQ, compresor, release con memoria, SC ▸ MIDS, aliasing, bypass, presets).

### Top Strip 737 V2

Plugin aparte (el 737 original se conserva igual). Misma base de circuito, más:

- **Respuesta medida con 9 impulsos del aparato** (MIC, INSTRUMENT en ganancia alta y baja, LINE, con EQ y compresor). De cada captura se guardaron solo números ajustados, nunca los archivos:
  - MIC: paso bajo de 1er orden a 16.9 kHz (igual en ganancia alta y baja).
  - INSTRUMENT: paso alto 3.2 Hz; paso bajo 21.1 kHz con HIGH GAIN y 46 kHz sin él.
  - LINE: plana (referencia).
- Las capturas confirman la forma del circuito: pasa altos de 6 dB/oct (ajuste de 1er orden, error 0.03 dB) y el EQ (shelves de 1er orden, campanas Q 0.2/0.85). Con los ajustes del panel encontrados por búsqueda, el plugin reproduce las 5 capturas con un error máximo de 0.04 a 0.29 dB (prueba `TopStrip2Test`).
- **MIX** (compresión en paralelo) en la barra superior: la señal directa se alinea con el circuito dentro del sobremuestreo, sin filtro de peine (-0.002 dB a 18 kHz con 50 %).
- Panel afinado: biseles achaflanados con brillos especulares, cara torneada con brillo radial, línea indicadora blanca, botones con bombilla interna (centro rojo-rosado, bordes oscuros y canto visible), sombras más definidas, serigrafía negra, aluminio más claro con grano marcado, placas esmaltadas con bisel, tornillos y tuerca del jack cromados, LED rojo como el original y rayas de escala que no se cruzan.

### Top Strip 737 V3

Plugin aparte (V1 y V2 se conservan). Mismo sonido que la V2 (respuesta calibrada con los impulsos). El trabajo de esta versión es el realismo y el manejo:

- **Perillas en capas**: el cuerpo 3D (bisel, pared moleteada, sombra de contacto) se renderiza una vez. La textura de mecanizado gira con la perilla, así el brillo cambia un poco según el ángulo. El indicador blanco se dibuja con la luz fija arriba a la izquierda, con sombra y destello en el canto.
- **Textura por sección**: preamp torneado, compresor cepillado radial, EQ granallado mate con cara levemente cóncava.
- **Animación suave a 60 fps**: las perillas siguen al valor con inercia (también al cambiar de preset o al deshacer). Los botones encienden y apagan como una bombilla (~90 ms), con luz que se escapa por la ranura. El LED y la lámpara del VU calientan y se enfrían.
- **VU**:
  - Retroiluminación cálida con punto caliente.
  - Aguja afinada con física de VU real (2do orden: 99 % en 300 ms y 1.5 % de sobrepaso, sobre la tensión lineal, con topes).
  - Sombra de la aguja sobre la escala, reflejo tenue en el cristal.
  - Zona roja de 0 a +3, escala de porcentaje y marcas cada 0.5 dB.
  - Sombra interior del bisel, viñeta, reflejo curvo, canto brillante del cristal y luz que se escapa al bisel.
- **Metal**: micro rayas en el aluminio y la banda de reflejo del cepillado.
- **Barra superior**:
  - Logo HM MUSIC (monograma propio) y nombre TOP STRIP 737 V3 con presencia.
  - Selector de presets en pantalla de cristal.
  - **A/B** con copia, **deshacer/rehacer** por gesto.
  - Carga **DSP** medida, LED de señal e indicador **OS 8x**.

> Top Strip 737 es una emulación con diseño y textos propios (marca "TOP STRIP" / "Dj Pava Hm Music"). No está afiliado a Avalon Design ni a Universal Audio; no se usan sus nombres, logotipos ni archivos.

---

## Top Drums

Procesador para el bus de batería (VST3 Windows/macOS + AU en macOS): transient shaper + color + glue + asistente de mezcla con IA.

| Sección | Controles |
|---|---|
| **Transient** | **Attack** (golpe), **Sustain** (cola), **Speed**, **Focus** (Full / Kick / Snare / Hats) y barra de transientes en vivo |
| **Color** | **Weight** (sub + armónicos del kick/808), **Drive** (saturación tipo cinta), **Tone** (oscuro ↔ brillante), **Clip** (recorte suave para drums pesados) |
| **Glue** | Compresor de bus: Threshold, Ratio (1.5/2/4/10), Attack (0.1–30 ms), Release (0.1–1.2 s o **Auto**), Makeup, **Glue Mix** (paralelo), **SC HPF** (el kick no hace bombear) y VU de GR |
| **Output** | Output, Mix global, Power (bypass) y VU |

Oversampling 2x de fase lineal en el color: el MIX en paralelo no produce cancelaciones.

**Presets por género:** Bad Bunny Heavy, Reggaeton Moderno, Reggaeton Clasico, Dembow, Trap Latino, Corridos Tumbados, Bachata, Salsa, Merengue, Cumbia, Drill, Afrobeats, Pop, Hip-Hop Boom Bap, Rock, Lo-Fi.

### AI Mix Assistant
- **IA local (gratis, sin internet):** elige el **género**, pulsa **ANALIZAR DRUMS** y reproduce 8 s. Mide pegada (pico/RMS), peso de graves, brillo, fuerza de los transientes y golpes por segundo, lo compara con el perfil del género y da recomendaciones. **APLICAR IA** pone los ajustes sugeridos.
- **Claude online (opcional):** pulsa **CLAVE API** y pega una clave de API de Anthropic (se crea en console.anthropic.com, se paga aparte por uso y es distinta de la suscripción de Claude). La clave se guarda solo en tu ordenador. **PREGUNTAR A CLAUDE** envía las medidas y tus ajustes (no envía audio) y muestra los consejos; **APLICAR CLAUDE** pone sus ajustes.

> Los presets "Style"/con nombre de artista son ajustes inspirados en el sonido general de esos géneros. No están afiliados ni aprobados por los artistas.

---

## Top Vocal Chain V2

Segunda versión con **el mismo motor de sonido** que Top Vocal Chain y una interfaz nueva, oscura con acentos neón, pensada para trabajar rápido. Es un plugin aparte (la versión 1 sigue igual).

- **Cuatro tarjetas de un clic** (LED = encender, flechas = estilo, engranaje = ajustes finos):
  - **Afinación**: Natural, Suave, Moderno, Robot.
  - **Tono**: Oscuro, Cálido, Neutro, Brillante, Aire.
  - **Espacio**: Seco, Sala, Plate, Hall, Eco.
  - **Carácter**: Limpio, Válvula, Cinta, Transformador, Fuego.
- **Compresor**, **De-Esser** (nuevo: baja solo las "S") y **Doubler**, cada uno con su lectura y engranaje.
- **Tonalidad** (nota, Mayor/Menor, medidor de cents), **Velocidad** y **Humanizar**.
- **EQ** de 4 faders (Graves, Medios, Agudos, Aire) con analizador y curva en vivo.
- **Reverb** y **Delay** con pantalla de puntos animada.
- **Aire**, **Armónicos**, **Saturación** y **Ancho**.
- Arriba: presets (clic en el nombre para la lista), **+** para guardar, **Antes/Después**, **A/B**, **UNDO/REDO**, **ASSIST** (la IA escucha 8 s y propone ajustes; tú decides si aplicarlos) y idioma **ES/EN**.
- Abajo: entrada, ayuda del control bajo el mouse, **MIX** y salida (arrastra los valores; doble clic = reset).
- Firma: *by Dj Pava Hm Music*.

---

## Top Vocal Chain V3

Tercera versión con el mismo motor y una interfaz de **cuatro módulos de color**, cada uno con su panel de detalle debajo. Plugin aparte (V1 y V2 siguen igual).

| Módulo | Tarjeta (flechas = estilo) | Panel de detalle |
|---|---|---|
| **DYNAMICS** (rosa) | Smooth, Punchy, Aggressive, Vintage, Clean, Modern · perilla = Makeup | **GATE** (fader; abajo = apagado), medidores **GR** (compresor) y **DS** (de-esser), **COMP** (a la derecha = más compresión), **RATIO**, **DE-ESS** y **FOCUS** |
| **TONE** (cian) | Natural, Warm, Bright, Airy, Dark, Radio | Analizador con curva, faders **LOW / MID / HIGH / AIR+**, **ST/MID** y filtros **Low Cut / High Cut** en un solo control |
| **SPACE** (morado) | Room, Plate, Hall, Massive, Echo, Slap · perilla = Width | Medidores de reverb y delay (IN / DUCK / OUT), **VRB DUCK**, **DLY DUCK**, **REVERB**, **DELAY**, **TIME**, **RATE**, **Ping Pong** y tono del espacio (oscuro ↔ brillante) |
| **SFX** (amarillo) | **Tape Sat, Tube, Radio, Lo-Fi, Crush** · perilla = nivel | **SFX** (cantidad), **ENHANCER** (aire sedoso + armónicos), **SPC POST** (reverb/delay después del efecto) y **FLT POST** (filtros también después del efecto) |

Arriba: UNDO/REDO, presets (clic en el nombre = lista), **+** guardar, **AI** (asistente que escucha la voz y propone ajustes) y **BYPASS**. Abajo: entrada y salida con medidores.

Novedades del motor (disponibles también por automatización en V1/V2): **Gate**, **SFX** con 5 efectos y rutas **SPC POST / FLT POST**.

---

## Top Blue Comp

Compresor óptico a válvulas estilo CL 1B (VST3 Windows/macOS + AU en macOS). El motor (`TubeOptoCompressor`) está basado en el código de Dj Pava HM:

- **Detección híbrida RMS/pico** (no distorsiona los graves).
- **Codo suave amplio de 10 dB**, típico de los compresores ópticos.
- **Balística óptica**: ataque suave y **release en dos etapas** (suelta rápido al principio y lento al final, como la memoria de la celda óptica), sin saltos.
- **Etapa de válvula** asimétrica (2º y 3er armónico) que colorea **sin subir el volumen**.
- **Transformador de salida**: +0.8 dB de calidez en 100 Hz.
- Procesado con oversampling 2x de fase lineal (el Dry/Wet en paralelo no cancela).

| Control | Qué hace |
|---|---|
| **GAIN** | Makeup / salida (-20 a +30 dB); empuja la válvula |
| **RATIO** | 2:1 a 10:1 |
| **THRESHOLD** | A la derecha = más compresión (0 a -40 dB) |
| **DRY / WET** | Compresión en paralelo |
| **ATTACK / RELEASE** | Rápido ↔ lento |
| **ATTACK/RELEASE SELECT** | **Fixed** (automático según la música, release en dos etapas), **Fix/Man** (ataque automático, release manual), **Manual** |
| **SIDECHAIN HPF** | Off, 60, 120, 180, 250, 500 Hz |
| **LR / M / S / Δ** | Estéreo enlazado, solo centro, solo lados, delta (escucha lo que se quita) |
| **COLOR** | Drive de la válvula (0 = limpio, 100 % = normal, 200 % = más color) |
| **VU GR / OUT** | El VU muestra reducción de ganancia o nivel de salida |

> El color está modelado matemáticamente (no son medidas de un equipo concreto). Top Blue Comp no está afiliado a Stam Audio ni a Acustica Audio.

---

## Top Choir

Crea un **coro a partir de una sola voz** (VST3 Windows/macOS + AU en macOS), diseño propio.

1. **Arrastra tu voz** (WAV, AIFF, FLAC o MP3) o pulsa **Buscar archivo**.
2. Elige el **Modelo** de coro: Gospel, Pop, Clasico, Urbano, Etereo o Iglesia (cuántas voces, cuánto se desafinan entre ellas, diferencia de tiempo, vibrato, formantes y sala).
3. Elige el **Micrófono**: Tubo Clasico, Condensador Estudio, Ribbon, Dinamico Broadcast o Valvula Vintage (color de EQ).
4. Añade **Octava Arriba / Abajo** y **Armonías**: **Tercera** y **Quinta**, arriba o abajo, **dentro de la tonalidad** (Tono + Escala; en el botón de Tono puedes **detectar la tonalidad** de tu voz).
5. **ST / MONO** y **Crear Voces**: genera el coro y lo guarda como WAV de 24 bits en *Documentos/Top Choir*.
6. **Arrastra el coro a FL Studio** desde el recuadro de resultado (o **Abrir carpeta**). Con **▶** lo escuchas en el plugin y con **M** lo escuchas junto a tu voz original.

Cada cantante del coro se crea con PSOLA (cambia la nota sin efecto "ardilla"), con su propia afinación, tiempo, formante, vibrato y lugar en el estéreo.

> Top Choir no está afiliado a ningún otro producto de coros.

## Top Voice MIDI

Convierte tu voz en MIDI en tiempo real: canta o tararea y salen notas; haz beatbox y salen golpes de batería.

- **Melodía**: detección de tono de baja latencia (~20-25 ms), velocidad según tu volumen, octava, respuesta y estabilidad.
- **Ritmo**: 4 pads (Bombo, Caja, Hi-Hat, Extra). Pulsa **ENTRENAR**, haz el sonido 5-10 veces y listo. Sin entrenar ya reconoce graves/medios/agudos.
- **Armonía**: tonalidad automática, 7 escalas, escala personalizada (toca las teclas o **APRENDER CON MI VOZ**) y acordes (mayor, menor, diatónico, 7ma, power, sus, maj7, min7, add9 o personalizado) con voicing e inversión.
- **Expresión**: pitch bend *Inteligente* (te sostiene en la nota) o *Exacto*, y 4 CC (envolvente, brillo, afinación, vocal a/u).
- **Captura**: graba el MIDI dentro del plugin, limpia notas fantasma, compensa la latencia y exporta un `.mid` que arrastras al DAW.
- **Sinte de prueba** integrado para oír las notas sin cargar instrumento.

Para tocar un instrumento de FL Studio en vivo: en el engrane del plugin → ajustes del wrapper → MIDI → *Output port* = 1, y en el instrumento *Input port* = 1. En otros DAW puedes usar loopMIDI y elegirlo en **Salida MIDI**.

> Top Voice MIDI es un diseño propio y no está afiliado a Vochlea ni a Dubler.

## Top Note Editor

Editor de notas por "blobs": ves cada nota cantada como una forma en un piano roll y la editas a mano.

1. **TRANSFERIR**: pulsa y dale play en tu DAW a la pista de voz; pulsa otra vez para terminar. La toma queda anclada a la línea de tiempo y al reproducir suena con tus ediciones en su sitio. También puedes **IMPORTAR** o arrastrar un WAV/AIFF/FLAC/MP3.
2. Algoritmo **Melódico** (voz e instrumentos de una nota) o **Percusivo** (batería: todas las notas en una línea).
3. Herramientas:
   - **Principal**: arriba/abajo = tono por semitonos (Alt = centésimas), izquierda/derecha = tiempo con cuadrícula (Alt = libre), bordes = duración, doble clic = afinar, doble clic justo encima = dividir (en la unión = unir).
   - **Modulación** (vibrato 0-200 %), **Deriva**, **Formantes** (timbre sin cambiar la nota), **Amplitud** (volumen y silenciar), **Separar**.
4. Macros: **Corregir afinación** (centro y deriva), **Cuantizar tiempo** (1/4 … 1/32 al tempo del DAW) y **Nivelar notas**.
5. Tonalidad/escala con **Ajustar a escala**, deshacer/rehacer, PLAY interno y **EXPORTAR WAV** para arrastrar al DAW.

Atajos: Ctrl+Z / Ctrl+Y, Ctrl+A, flechas (tono y tiempo), Supr (silenciar), espacio (play), Ctrl+rueda (zoom).

> Top Note Editor es un diseño propio y no está afiliado a Celemony ni a Melodyne. No incluye el algoritmo polifónico (acordes) ni el editor de sonido espectral.

### Motor nuevo (v2)
- Las notas editadas se procesan en **islas continuas**, con transiciones de tono suaves entre notas. Antes cada nota se cortaba y se pegaba, y eso causaba clics y "voces dobles" en los bordes.
- **Re-alineado**: en los bordes de cada isla, el audio procesado vuelve a coincidir en fase con el original (−42 dB de diferencia en el empalme). Lo que no editas queda idéntico.
- **Control de volumen**: al subir o bajar el tono, la nota conserva su volumen original (antes perdía ~1.7 dB).
- Marcas de tono alineadas por correlación, igual que el HIGH Q de Top Tune.
- **Navegación**: barras de desplazamiento abajo y a la derecha, ← → (Shift = página), Inicio/Fin, Shift+rueda o rueda sobre la regla, Alt+arrastrar o botón central (mano). Además, la vista sigue la reproducción.
- **Arrastrar al DAW** (botón naranja abajo a la derecha): agárralo y suéltalo en la lista de reproducción. Crea la voz afinada sola (y la reutiliza si no cambiaste nada). Con **"Empezar en el compás 1"**, el WAV trae silencio hasta donde empezaba la toma, así que al soltarlo en el compás 1 queda alineado con la voz original. Un clic sin arrastrar abre la carpeta del archivo.

## Top Smooth

Supresor dinámico de resonancias: encuentra las frecuencias que "pican" (dureza, silbidos, resonancias de sala o de micro) y las baja solo mientras suenan, sin apagar el resto.

- **Profundidad** (cuánto baja), **Detalle** (cortes estrechos en los picos más marcados ↔ cortes amplios y suaves), **Ataque**, **Release**, **Corte máx**, **Inclinación**, **Mezcla** y **Trim**.
- Modos **Suave** (umbral adaptativo, el más transparente) y **Duro** (más preciso y fuerte).
- **Fase lineal** (máxima calidad, latencia compensada por el DAW) o **Baja latencia (0 ms)** para grabar o en vivo.
- **Foco**: 5 puntos arrastrables en la gráfica para trabajar más o menos en cada zona, más cortes grave/agudo.
- Estéreo **Enlazado**, **L / R** o **Mid / Side**, **sidechain** externo, **Delta** (escucha solo lo que se quita) y 10 presets.

> Top Smooth es un diseño propio y no está afiliado a oeksound ni a soothe.

## Top Master

Suite de mastering todo en uno con asistente.

- **Asistente de mastering**: eliges género (Pop, Hip-Hop/Trap, Reggaetón/Urbano, Dembow, Tropical, EDM, Rock, R&B, Acústico), volumen final (Streaming -14, -11, -9, Club -7.5 LUFS) e intensidad; pulsas **ESCUCHAR**, le das play a la parte más fuerte y arma la cadena: EQ hacia el balance tonal del género, dinámica de 3 bandas, graves en mono, imagen estéreo, brillo y el Maximizador con la ganancia exacta para llegar al LUFS (simula la cadena antes de aplicarla). También puede usar **tu canción de referencia** como objetivo de tono y volumen.
- **Módulos** (se pueden reordenar): Ecualizador 8 bandas (estéreo/Mid/Side), EQ Dinámico, Moldeador Espectral, Dinámica multibanda, Control de Graves, Impacto, Deslimitador, Claridad, Excitador, Cinta Vintage, Imagen Estéreo, **Maximizador** true-peak (Transparente, Punch, Agresivo, Clipper) y Dither.
- **Medidores**: LUFS integrado / corto / momentáneo, true peak, reducción del maximizador y correlación.
- **Referencia A/B** con **igualar volumen**, y bypass igualado para comparar de forma justa.

> Top Master es un diseño propio y no está afiliado a iZotope ni a Ozone. No incluye separación de stems (Stem EQ / Master Rebalance) ni Codec Preview.

## Top Rack

Rack de mezcla modular: 8 ranuras donde pones los módulos que quieras, en el orden que quieras.

- **15 módulos**: EQ Clase A (estilo británico con dos medios), EQ Consola (4 bandas, tipo Negro/Marrón), EQ Americano (pasos con Q proporcional), EQ de Aire, Comp FET (con modo "Todos"), Comp VCA, Comp Óptico, Comp Bus (auto release), Estresador (ratios por pasos, Nuke, Dist 2/3), Monstruo (paralelo extremo), De-Esser, Puerta, Revivir (Brillo/Cuerpo), Bombardero (Golpe/Cuerpo/Crunch/Mezcla) y Trim.
- **Por ranura**: encendido, **trim** de entrada, **sidechain externo** para los módulos de dinámica, subir/bajar y quitar.
- **4 Macros**: clic derecho en cualquier perilla → asignar a una macro; una perilla mueve varias a la vez.
- **8 Dream Strips** para guardar y cargar cadenas con un clic, **11 presets de cadena** (voz pop, voz urbana, rap, bombo, caja, bajo, guitarra, bus de batería, mix bus, piano), **oversampling 2x**, entrada, salida y mezcla global.

> Top Rack es un diseño propio inspirado en los racks modulares de mezcla; no está afiliado a Slate Digital ni usa sus nombres ni gráficos. Los módulos imitan el estilo de equipos clásicos, no son modelados medidos de un equipo concreto.

- **Interfaz estilo rack de hardware**: los módulos van lado a lado (desplazamiento horizontal), cada uno con su propio frente (VU, LEDs, perillas estriadas, de tapa de color, cromadas o "pico de pollo").
- **Navegador** a la izquierda con miniaturas, categorías y buscador: clic o arrastra un módulo al rack. Pestaña **Macro** con las 4 macros.
- Por ranura: encendido, **S** (solo), menú del módulo (cambiar, mover, quitar), **OUT GAIN** y sidechain **INT/EXT**.
- Barra superior: cadena de presets, **UNDO/REDO**, **A/B** con copia, vaciar rack, **HQ** y entrada/salida/mezcla. Fila de **Dream Strips**.

## Top Console B

Canal de consola estilo serie 4000 "B" (diseño y nombre propios, sin relación con ningún fabricante).

- Interfaz estilo consola: filtros y EQ a la izquierda, medidores de entrada/salida, fader VCA al centro y dinámica a la derecha.
- **Entrada**: in trim ±20 dB, DRIVE (preamp con transformador, compensado en volumen) con botón pre, inversión de fase y modo HQ (oversampling 2x).
- **Imagen**: width (0–2), pan, MONO y CUT.
- **Filters**: HPF 18 dB/oct (16–350 Hz) y LPF 12 dB/oct (3 kHz–22 kHz), con envío al sidechain.
- **Equaliser** de 4 bandas: LF y HF shelf/campana (BELL), LMF y HMF paramétricos de Q constante, con gráfica en vivo y envío al sidechain.
- **Dynamics**: compresor de realimentación (ratios 1…∞ y posición **ds** de de-esser) con **mix** paralelo, LEDs de atenuación, gate/expansor con range, threshold y release; botones de ataque rápido.
- **Visor de ruteo**: qué alimenta el sidechain (filtros, EQ, externo), orden de los procesos y escucha del sidechain (listen s/c).
- Barra inferior: UNDO/REDO, comparación A/B, navegación de presets y HQ.
- **Output**: fader VCA de +12 a −∞ (color sutil por encima de 0 dB), OUT TRIM, CUT y medidores LED de pico/RMS.
- 11 presets: Canal limpio, Voz al frente, Voz urbana, De-esser de voz, Bombo, Caja, Bajo, Guitarra acústica, Overheads, Bus de batería y Piano.

## Top Meter

Medidor profesional de sonoridad y nivel (diseño propio, sin relación con ningún fabricante).

- **Sonoridad ITU-R BS.1770-4 / EBU R128**:
  - Integrated (con compuertas), Short-Term, Momentary y sus máximos;
  - LRA (EBU Tech 3342), PLR y PSR;
  - verificado con las señales de prueba EBU Tech 3341/3342 (±0.02 LU).
- **True Peak** con sobremuestreo x4.
- Gráfica de historia (10 s a 10 min) con objetivo, histograma y vista general para revisar lo anterior.
- Objetivos: -9, -14 (Spotify/YouTube), -16 (Apple Music), -18 (podcast), -23 (EBU) y -24 LUFS (ATSC).
- **Level**: barras L/R de pico o true peak con retención, cajas de pico y RMS (máx/actual), goniómetro, balance y correlación.
- **VU** analógicos L/R con referencia ajustable (0 VU = -18 dBFS por defecto).
- **Espectro** en tercios de octava (31 bandas) con picos, promedio y lectura por banda.
- DAW Sync (reinicia al dar play), pausa, reset, nombre de canal editable, UNDO/REDO, A/B y presets.

## Top Mini CH

Canal compacto estilo módulo 500 de mini consola (diseño y nombre propios, sin relación con ningún fabricante).

- **INPUT**: GAIN ±20 dB (preamp limpio), Ø, **COLOR** (armónicos analógicos, en el lugar del 48V del hardware), filtro pasa altos **75 Hz 12 dB/oct**, medidor de 5 LED en dBu (0 dBu = -18 dBFS).
- **EQ** de 2 bandas ±15 dB: HF shelf 3.5 kHz o campana 5 kHz (BELL); LF shelf 60 Hz o campana 200 Hz (BELL); EQ IN.
- **COMP** de una perilla: THRESHOLD +10 a -20 dBu, relación fija 2:1 con rodilla suave, feed-forward, ataque según el programa y release fijo, compensación automática suave y LEDs de reducción (verde/amarillo/rojo).
- **[LINE]** (9 dB menos de ganancia) y **Hi-Z** (carácter de entrada de instrumento).
- 10 presets (clic derecho o clic abajo en "MiNi").

## Top Mic Collection

Emulador de micrófonos clásicos (nombres y diseño propios; cada modelo se inspira en el carácter sonoro general de un clásico de estudio).

- **15 micrófonos**:
  - 6 de válvula: TM-V1 a TM-V6 (TM-V6 con el carácter de un Manley Reference);
  - 3 FET: TM-F1 a TM-F3;
  - 3 dinámicos: TM-D1 a TM-D3;
  - 3 de cinta: TM-R1 a TM-R3.
  - Cada uno trae su curva, sus patrones disponibles, su efecto de proximidad, su pérdida fuera de eje y su saturación (válvula/transformador).
- **SOURCE MIC**: le dices con qué grabaste (dinámico, de radio, condensador económico, USB, celular, condensador pequeño, **Neumann TLM 103** o **AKG C414 XLII**) y quita ese color antes de aplicar el modelo.
- **PATTERN** (omni, wide, cardioide, hiper, figura 8), **FILTER** (60/100/160 Hz), **AXIS** (0–180°), **PROXIMITY** (±100 %), **OUTPUT**, **REV** (parte de atrás) y **Ø**.
- Pantalla polar con el patrón y la posición de la voz.
- **DUAL**: dos micrófonos a la vez con mezcla A/B.
- 16 presets (4 pensados para grabar con TLM 103 o C414 XLII).

## Top Locker

Colección virtual de 12 micrófonos (versión 2.0) con preamp y sala integrados. Cada micrófono no es solo una EQ: tiene su propia **respuesta en frecuencia**, **saturación y armónicos** (válvula = pares, dinámico y cinta = impares), **transientes**, **compresión natural** en niveles altos, **efecto de proximidad** y **cuánta sala capta**.

- **Micrófonos** dibujados al estilo foto (malla cuadrada tejida, níquel mate, suspensión elástica, sombras y reflejos), con nombres propios; debajo de cada uno dice a cuál imita: Studio Silver (U87 Ai), Modern Silver (TLM 103), Gold Detail (C414 XLII), Crown Tube (C-800G), Classic Tube (ELA M 251), Black Reference (Manley Reference), Broadcast Black (SM7B), Ribbon Modern (R-121), Ribbon Classic (4038), Air Tube (C12), Velvet Tube (M49), Voice Line (RE20). Al elegir uno se enciende un foco de luz.
- **SOURCE MIC**: le dices con qué grabaste (incluye **Neumann TLM 103** y **AKG C414 XLII**) y le quita ese color antes de poner el nuevo micrófono.
- **Mic Position**: ángulo de 0° (en eje) a 90°, con vista desde arriba donde el micrófono gira y se ve su patrón polar.
- **Proximity**: distancia de ~2 a ~60 cm, con vista de lado (persona y micrófono) y zonas Close / Medium / Far.
- **Tone Control**: Low (60/100/200/300 Hz, Shelf o Bell), Mid (400 Hz a 5 kHz), High (5 a 16 kHz, Shelf o Bell) y curva de EQ en pantalla junto al color del micrófono.
- **Character**: circuitos Clean, Modern, Vintage, Tube, Transformer, Analog y Smooth, con cantidad Clean ↔ Color.
- **Mic Preamp**: N-73 (Neve-style), A-12 (API-style), S-4K (SSL-style), T-76 (Tube-style), M-70 (Modern clean), V-610 (Vintage tube) o Clean, con **Drive** (color) e **Input** (nivel que entra: más saturación, salida compensada).
- **Room**: Vocal Booth, Studio A, Studio B, Live Room, Hall, Plate, Chamber, Habitación vintage o seco, con Amount, Size, Decay, Early Reflections, Width y Damping.
- 19 presets (Male/Female Vocal, Trap, Dembow, Reggaeton, Rap, R&B, Pop, Balada, Drill, Afrobeat, Podcast, Rock, Coro, Plate, Transformador, Lofi 610...). Los presets no cambian tu SOURCE MIC. **Save/Load** para tus propios presets.
- Sobremuestreo 2x (49 muestras de latencia, compensada). Ventana escalable.

## Top EQ-1A

Ecualizador pasivo a válvulas con el panel distribuido como el original de los años 50 (placa azul pizarra pintada, perillas de baquelita con falda y estrella, perillas de barra para CPS y KCS, selector de pico para ATTEN SEL, palanca **IN** que saca solo el EQ, lámpara piloto de joya y perilla **OFF/ON**). El texto de marca es propio ("TOP / PROGRAM EQUALIZER / EQ-1A").

**No es una copia de curvas: es una simulación del circuito.** La red pasiva completa (resistencias, condensadores, bobina con tomas y potenciómetros) se resuelve por análisis nodal con reglas trapezoidales a 8x, así que las secciones interactúan igual que en el aparato. La topología sigue el análisis publicado del circuito original (Barrera, Lizarraga-Seijas y Font, SMC 2024) y los valores se calibraron contra el manual y las mediciones de Sound On Sound (2019):

| Medida | Original | Top EQ-1A |
|---|---|---|
| LOW BOOST máximo | 13.5 dB | 13.0-13.3 dB |
| LOW ATTEN máximo | 17.5 dB | 17.0-17.5 dB |
| HIGH BOOST máximo (SHARP) | 18 dB | 17.8-18.3 dB |
| SHARP vs BROAD | ~9 dB | 8.8 dB |
| HIGH ATTEN máximo | 16 dB | 16.4 dB |
| Truco 2.5/2.5 a 60 Hz | +3 / -2 dB | +2.7 / -1.4 dB |
| Truco 10/10 a 60 Hz | hueco ~6 dB cerca de 600 Hz | -5.3 dB en 549 Hz |
| THD a +10 dBm | 0.15% | 0.148% (domina el 2º armónico) |
| Salida máxima | +21 dBm | +20.5 dBu a 1% THD |
| Respuesta | 20 Hz-20 kHz +0/-1 dB | -1.1 dB a 20 Hz, -0.4 dB a 20 kHz |

- Amplificador 12AX7 → 12AU7 en contrafase con transformadores que saturan por flujo (los graves fuertes ganan peso). Calibración: 0 dBFS = +18 dBu. A nivel normal queda limpio (0.04%).
- **DRIVE** (±12 dB, volumen compensado) para empujar las válvulas y **OUTPUT** ±12 dB, en la barra superior junto a los presets.
- Las perillas se mueven sin clicks: el circuito guarda la carga real de cada condensador y bobina.
- 12 presets (voz, rap, bombo, 808, guitarra, overheads, master, lofi). Sobremuestreo 8x con filtros de -110 dB (sin aliasing al saturar).

## Top 73

Preamp + EQ clase A con transformadores (estilo 1073). **Panel vertical de módulo de consola**, dibujado con el renderizador 3D por píxel y medido sobre una foto frontal del módulo clásico: panel azul pizarra, perilla roja de dos alas con falda de acrílico transparente (MIC 20-80 / LINE -20 a +10, con sus marcas OFF), agudos en perilla gris moleteada, medios y graves con **aro blanco de frecuencia** y perilla gris de ganancia, filtro en perilla azul de dos alas, teclas blancas **EQL** y **PHASE**, tornillos y tornillo de mano. Logotipo propio ("HM"), sin marcas ajenas. Al lado, una bandeja del mismo estilo con el nombre, pantalla de presets, medidor de LEDs (I/P - O/P), **LEVEL**, **AUTO** y **POWER**. Ventana escalable (proporción fija).

**Motor 2 (rehecho desde cero), todo a 8x y en doble precisión:**

- **El EQ simula el circuito**, no son filtros "parecidos": análisis nodal modificado + integración trapezoidal en tiempo real, con los valores de componentes del esquema (resistencias, condensadores, inductores y la etapa inversora). Validado contra las curvas medidas de un clon que sigue el original (error ~1 dB) y contra la descripción técnica del original:
  - **Graves** (35/60/110/220 Hz, ±16): shelving tipo Baxandall con su rebote bajo la frecuencia elegida y la pequeña "muesca" por encima.
  - **Medios** (0.36/0.7/1.6/3.2/4.8/7.2 kHz, ±18): inductor + condensador en serie. **Q proporcional** (poca ganancia = campana ancha) y más estrecho en frecuencias altas, como el original.
  - **Agudos 12 kHz** (±16): shelf de pendiente muy suave; con +16 empieza a subir desde 1 kHz (+9 dB en 3 kHz) y con +6 queda igual a la medición por impulso de un 1073 real (+0.4 en 3 kHz, +2.9 en 10 kHz, +5.5 en 20 kHz). Muesca de ~1 dB en 330 Hz al reforzar fuerte.
  - **HPF** 50/80/160/300 Hz: red pasiva C-L-C de 3er orden (18 dB/oct) cargada con 5k1.
  - Las perillas leen dB reales (graves = ganancia en el estante, medios = ganancia en el centro, agudos = ganancia en 12 kHz).
- **Preamp clase A con realimentación**: cada etapa es un transistor (curva exponencial: corte suave abajo, saturación arriba) dentro de un lazo de realimentación. De MIC 55 a 80 entra la segunda etapa y hay menos realimentación: más 2º armónico y más armónicos altos, como mide un 1073 real. Calibrado: THD 0.05% a +20 dBu en 1 kHz y 0.066% en 50 Hz (spec ≤ 0.07%), 2º y 3º parecidos a ganancia baja, salida máxima +26.25 dBu, -85 dB a nivel normal. MIC 80: 0.69% con más 2º que 3º.
- **Transformador de salida con entrehierro (estilo LO1166)**: flujo magnético con histéresis tipo Rayleigh (da peso en graves también a nivel normal: 0.04% en 20 Hz a -18 dBFS), saturación cúbica y un término par por la corriente continua de la etapa clase A. 0.39% en 20 Hz a +20 dBu.
- **Respuesta**: ±0.5 dB de 20 Hz a 20 kHz y -3 dB en 40 kHz (spec), leve rebote en graves por el transformador. A ganancia alta se mantiene lo medido por impulso en un 1073 vintage: resonancia en ~13 Hz y agudos que se abren (+5.6 dB en 20 kHz en MIC 80).
- **Perilla roja**: MIC 20-80 y LINE -20 a +10, en pasos de 5 dB. **AUTO** compensa el volumen para que solo cambie el color. **LEVEL**, medidor de LEDs (I/P u O/P), **POWER**, teclas **PHASE** y **EQL**, 11 presets. Sin aliasing al saturar fuerte (-117 dB).

## Top VoxStrips (antes 6961P)

Canal de voz a válvulas con el panel copiado de las fotos del original: aluminio azul acero con cepillado de pelo fino, placas negras anodizadas también cepilladas, serigrafía gris clara de peso medio, perillas negras con barra central mate y dos conchas laterales satinadas, palancas cromadas tipo bat con tuerca, jack de níquel negro con aro cromado, tornillos allen en aro negro, VU ámbar con la escala medida en la foto (más plana que un círculo) y aguja con física de 2do orden a 60 Hz. El nombre grabado es propio ("TOP VOX / STRIPS"). **Mismo código de plugin que el 6961P**: los proyectos guardados abren igual (borra el `6961P.vst3` viejo para no ver los dos).

Cadena en el orden del hardware, **toda a 8x y en doble precisión**: **HPF → INPUT → compresor óptico ELOP 3:1 (antes del preamp) → preamp a válvula (40-60 dB) → EQ pasivo → de-esser LC / limitador 10:1 → etapa final a válvula (5751 + 6414) → XFMR → salida**.

- **Preamp**: GAIN 40/45/50/55/60, INPUT ±20 dB, filtro de graves 6 dB/oct en 80 o 120 Hz, fase 0°/180°.
- **Preamp calibrado con la especificación del VOXBOX** (0 dBFS = +22 dBu, o sea -18 dBFS = +4 dBu): THD del preamp 0.27% a +20 dBu (spec 0.3%), preamp + EQ 0.34% (spec 0.35%), salida máxima +29.5 dBu a 1.5% THD (spec ~+30), respuesta -0.5 dB en 10 Hz (spec ±1 dB). Como en el original, GAIN **no es un pad**: cambia la realimentación de la válvula. 40 = limpio (0.09%), 50 = normal, 60 = caliente y al frente (0.86%, más 2º armónico). Las curvas de válvula usan ADAA (antiderivada en tabla) a 8x: componentes fuera de armónicos a -138 dB saturando fuerte.
- **Compresor ELOP 3:1** con compresión "paramétrica" de cuatro constantes de tiempo: ataque (4, 8, 15, 30, 70 ms) más rápido cuanto más fuerte es el pico, release rápido del pico, "plataforma" que se carga con la compresión sostenida y se suelta con RELEASE (0.3, 0.5, 1, 2, 5 s), y memoria de la celda (frases largas vuelven más lento, golpes cortos rápido, frenado suave cerca de 0 dB). RELEASE también cambia el ataque, como en el original. Calibrado con la hoja de datos: con THRESHOLD al máximo y un seno de +4 dBu reduce 8 dB; reducción máxima 16 dB. LINK/SEPARATE para estéreo.
- **EQ pasivo**: LOW PEAK 0 a +10 dB (20 Hz-1 kHz), MID DIP 0 a -10 dB (200 Hz-7 kHz), HI PEAK 0 a +10 dB (1.5-20 kHz), 11 frecuencias cada uno, XFMR IN (transformador de salida). **La forma de las bandas está ajustada a 14 mediciones por impulso** (paquete "VOXBOX IRS" de PastToFutureReverbs; los archivos no se incluyen): LOW Q 0.6, MID Q 1.1, HI Q 0.8. Ahora se calcula desde el prototipo analógico a 8x (sin compresión de frecuencias cerca de 20 kHz). 13 presets llevan el nombre y el ajuste de cada medición.
- **De-esser LC / limitador** (después del EQ, antes de la etapa final): 3K, 6K, 9K o 12K con una red LC pasiva en serie con la fotorresistencia (con poca reducción la muesca es ancha y suave, con mucha es profunda y estrecha), 10:1, ataque 2 ms, release 0.5 s, hasta 32 dB. LIMIT 10:1 calibrado: 5 dB de reducción con +4 dBu y THRESHOLD al máximo.
- **VU** con 5 lecturas: G-R, LINE IN, PRE OUT, EQ OUT y D-S. POWER, lámpara READY y OUTPUT. El interruptor de 48V y el jack son solo decorativos.
