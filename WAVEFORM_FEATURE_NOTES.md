# Notatki: wyświetlanie zdekodowanych danych jako ciągłego wykresu (waveform)

Data: 2026-08-19
Status: **implementacja przeszła pierwszy pełny przebieg — buduje się, backend
zweryfikowany testem, rysowanie i UI niezweryfikowane wizualnie**

> ## Stan na koniec sesji 2 (WSL)
>
> Build na WSL działa — przepis w sekcji "Build na WSL" na końcu pliku.
> Zaimplementowane i skompilowane:
>
> 1. `libsigrokdecode4DSL/decoders/pwm/pd.py` — dwa nowe `OUTPUT_META`:
>    `Duty cycle` (%) i `Period` (s), emitowane per cykl (`ss_block..es_block`).
>    Istniejący `Average` zostawiony bez zmian (ma narastający span, nie nadaje
>    się na wykres per cykl).
> 2. `DSView/pv/data/decode/metadata.h/.cpp` — **nowe.** `MetaData` = jeden
>    strumień pomiarowy: rzadkie próbki `(start, end, value)`, własny mutex,
>    flagi `shown`/`interp`. Klucz `MetaId{const srd_decoder*, pdo_id}`.
> 3. `DSView/pv/data/decoderstack.h/.cpp` — rejestracja callbacku
>    `SRD_OUTPUT_META`, `meta_callback()`, `get_meta_streams()`, czyszczenie
>    próbek w `init()`, zwalnianie w destruktorze.
> 4. `DSView/pv/view/decodetrace.h/.cpp` — `draw_meta_waveform()`, wiersz
>    wykresu w `paint_mid()`, `rows_size()` liczy wiersze wykresów,
>    `_cur_row_units` (nagłówki nie mogą już być rozstawione co stałą wysokość).
> 5. `DSView/pv/widgets/decodergroupbox.h/.cpp` — per strumień: przycisk
>    pokaż/ukryj + combo Step/Linear.
> 6. `CMakeLists.txt` — dodany `metadata.cpp`.
>
> **Zweryfikowane:** cały projekt buduje się bez błędów; aplikacja startuje i
> ładuje dekodery; pipeline META działa end-to-end — harness na prawdziwym
> libsigrokdecode (`scratchpad/meta_harness.c`) na syntetycznym PWM z rampą
> wypełnienia zwrócił `Duty cycle` = 14, 18, 22, 26 … 66%, `Period` = 1e-4 s,
> trzy strumienie rozróżnione po `pdo_id` (2=Average, 3=Duty, 4=Period).
>
> **NIEzweryfikowane:** czy wykres faktycznie się rysuje na ekranie i czy
> przełączniki w dialogu opcji działają. Brak `xdotool` na tej maszynie, więc
> nie dało się zautomatyzować klikania (start Demo → dodaj dekoder PWM →
> opcje → przełącz wykres). To jest pierwsza rzecz do zrobienia w kolejnej sesji.
>
> **Decyzje projektowe podjęte w sesji 2:**
> - Interpolacja: **oba tryby**, przełączane w UI per strumień (Step domyślnie).
> - Umiejscowienie: **osobny wiersz**, ale zaimplementowany *wewnątrz*
>   `DecodeTrace`, a nie jako nowa klasa `Trace` w stylu `MathTrace`. Powód:
>   `View::signals_changed` w trybie LOGIC przebudowuje listę traces wyłącznie
>   z `SR_CHANNEL_LOGIC` i `SR_CHANNEL_DECODER` (`view.cpp:787-809`) — nowa
>   klasa Trace zniknęłaby z layoutu dokładnie w trybie analizatora logicznego.
> - Skala Y: **auto-fit** do min/max widocznego zakresu.
>
> **Znane ograniczenie:** strumienie META powstają dopiero, gdy dekoder je
> wyemituje (libsigrokdecode tworzy outputy w pythonowym `start()`, nie ma API
> do wylistowania ich z góry). Więc przy pierwszym dodaniu dekodera lista
> wykresów w dialogu jest pusta — pojawia się dopiero po pierwszym dekodowaniu.


## Cel

Dziś, gdy dekoder protokołu (np. `pwm`) wylicza wartość liczbową (np. wypełnienie PWM
w %), DSView pokazuje ją wyłącznie jako tekst w "pigułce"/prostokącie adnotacji w wierszu
dekodera (tak jak PulseView). Chcemy dodać opcję pokazywania takiej wartości jako
**ciągłego wykresu (waveform)** zsynchronizowanego z osią czasu — analogicznie do
istniejących kanałów analogowych / kanału `Math`.

Docelowo ma to działać **generycznie dla dowolnego dekodera** z liczbowym wyjściem,
nie tylko dla PWM. PWM będzie pierwszym dekoderem użytym jako przykład/test.

## Decyzje podjęte w rozmowie z użytkownikiem

- **Architektura: pełne podłączenie `SRD_OUTPUT_META`** (opcja "właściwa", nie hack z
  nadużyciem istniejącej konwencji `PyLong` w adnotacjach — tamta obsługuje tylko liczby
  całkowite i jest przeznaczona do formatowania bin/hex/oct/dec wartości magistrali,
  nie do pomiarów zmiennoprzecinkowych).
- **Zakres: generyczny mechanizm** dla wszystkich dekoderów z zarejestrowanym wyjściem
  `OUTPUT_META`, a nie implementacja na sztywno dla samego PWM.
- **Build/test: przenosimy się na WSL.** Build na Windows jest niestabilny — wg
  użytkownika nikomu z innych kontrybutorów nie udało się go tam poprawnie zbudować.
  Ten katalog repo zostanie przeniesiony na WSL i tam będzie liczona kompilacja
  i testowanie GUI (Qt).

## Co już istnieje w kodzie (ustalenia z eksploracji)

Repo root: `D:\code\DSView` (na WSL ścieżka się zmieni, ale struktura relatywna zostaje
taka sama).

### 1. Pipeline adnotacji tekstowych (`SRD_OUTPUT_ANN`) — działający dziś

- `DSView/pv/data/decode/annotation.h` / `.cpp` (82+190 linii) —
  `pv::data::decode::Annotation`. Trzyma `_start_sample`, `_end_sample`, `_format`
  (= klasa adnotacji, wybiera `Row`), `_type`, oraz `_resIndex` — indeks do
  deduplikowanej tabeli zasobów `AnnotationResTable` (żeby nie duplikować tekstu w
  pamięci). Nie trzyma tekstu bezpośrednio.
- `DSView/pv/data/decode/annotationrestable.h` / `.cpp` (72+399) —
  `AnnotationSourceItem` (annotationrestable.h:32-40): `bool is_numeric`,
  `char *str_number_hex`, `src_lines`/`cvt_lines` (tekst oryginalny / przeformatowany
  bin-hex-oct-dec). `AnnotationResTable::format_numberic()` (annotationrestable.cpp:228)
  konwertuje hex string na tekst w wybranym formacie — **czysto do wyświetlania**, nie
  ma tu accessora zwracającego `double`/`long long`.
- `DSView/pv/data/decode/row.h/.cpp` (76+102) i `rowdata.h/.cpp` (77+157) — `Row` grupuje
  adnotacje pod nazwanym wierszem wyjścia dekodera (`srd_decoder_annotation_row*`);
  `RowData` trzyma `std::vector<Annotation*>`, ma `get_annotation_subset(start,end)`
  używane przy rysowaniu.
- `DSView/pv/data/decoderstack.cpp:747-751` — jedyna zarejestrowana obsługa callbacku:
  ```cpp
  srd_pd_output_callback_add(session, SRD_OUTPUT_ANN,
                              DecoderStack::annotation_callback, _stask_stauts);
  ```
- `DecoderStack::annotation_callback(srd_proto_data*, void*)` — `decoderstack.cpp:784-842`.
  Wywoływane synchronicznie przez libsigrokdecode dla każdego `self.put(..., self.out_ann,
  [...])` w Pythonie. Tworzy `new Annotation(pdata, d->_decoder_status)`, znajduje właściwy
  `Row` po `(srd_decoder*, ann_class)` w mapach `_class_rows`/`_rows`, i robi
  `row_iter->second->push_annotation(a)`.

### 2. Rysowanie adnotacji (tekst w "pigułkach")

`DSView/pv/view/decodetrace.h` (201) / `decodetrace.cpp` (723), klasa
`pv::view::DecodeTrace : public Trace`:

- `paint_mid()` (decodetrace.cpp:215-313) — iteruje wiersze (`Row`), woła
  `_decoder_stack->get_annotation_subset(...)`, potem `draw_annotation()` dla każdej.
- `draw_annotation()` (326-404) — liczy piksele start/end, kolor wg `a.type()`, wybiera
  `draw_instant()` (zdarzenie punktowe, 431-450) lub `draw_range()` (452-511, klasyczny
  sześciokąt z tekstem wewnątrz). **To tu trzeba dodać alternatywną ścieżkę rysowania,
  gdy wiersz jest przełączony w tryb "wykres".**
- `draw_nodetail()` (406-429) — placeholder "Zoom in for details" przy dużym oddaleniu.

### 3. Istniejąca (ograniczona!) obsługa liczb w adnotacjach — **ślepy zaułek dla floatów**

Konwencja: dekoder Pythona może w liście adnotacji przekazać `int` (`PyLong`) obok
tekstu; C zapisze go jako hex string.

- `libsigrokdecode4DSL/type_decoder.c:58-168`, funkcja `py_parse_ann_data()`:
  ```c
  else if (PyLong_Check(py_tmp)) { py_numobj = py_tmp; }
  ...
  if (py_numobj != NULL) {
      lv = PyLong_AsLongLong(py_numobj);
      sprintf(hex_str_buf, "%02llX", lv);
      *numberic_value = lv;
  }
  ```
- `libsigrokdecode.h:348-354`, `struct srd_proto_data_annotation`: pole
  `long long numberic_value` — **tylko całkowite**, brak floatów.
- Wniosek: ta ścieżka jest zaprojektowana do formatowania wartości bajtów/słów magistrali
  (bin/hex/oct/dec), **nie nadaje się** do pomiarów typu wypełnienie PWM (%) bez
  sztucznego skalowania (np. `int(duty*100)`) — dlatego odrzucono ją jako architekturę
  docelową.

### 4. Rysowanie przebiegów analogowych / matematycznych — wzorzec do naśladowania

- `DSView/pv/data/analogsnapshot.h/.cpp` (131+437) — `AnalogSnapshot`. Surowe próbki w
  buforze `void *_data` (1 bajt/próbkę, kody ADC), plus piramida mip-map min/max
  (`EnvelopeSample{min,max}`, `ScaleStepCount=10`, `EnvelopeScaleFactor=16`) budowana w
  `append_payload_to_envelope_levels()` (276-368) — do szybkiego rysowania przy dużym
  oddaleniu.
- `DSView/pv/view/analogsignal.h/.cpp` (185+686), `AnalogSignal : public Signal`:
  - `paint_mid()` (l.395) — wybór tryb pełnej rozdzielczości vs. envelope wg
    `samples_per_pixel < EnvelopeThreshold`.
  - `paint_trace()` (l.472) — pełna rozdzielczość: `x = start_pixel + sample*px_per_sample`,
    `y = zeroY + (yvalue - hw_offset) * _scale`, `p.drawPolyline()`.
  - `paint_envelope()` (l.525) — tryb oddalony: `QRectF` per kolumna pikseli wg
    min/max z `AnalogSnapshot::get_envelope_section()`.
- **Najbliższy istniejący wzorzec dla wartości "obliczonej", nie sprzętowej**:
  `DSView/pv/data/mathstack.h/.cpp` (155+531) — `MathStack : public QObject, public
  SignalData`, liczy `add/sub/mul/div` dwóch kanałów DSO, trzyma wynik jako
  `std::vector<double> _math` (prawdziwy float) + piramidę envelope na double.
  `calc_math()` (mathstack.cpp:361).
  `DSView/pv/view/mathtrace.h/.cpp` (170+541) — `MathTrace : public Trace`, rysuje tak
  jak `AnalogSignal`, czytając `_math_stack->get_math(start)`:
  ```cpp
  // mathtrace.cpp:296
  const float y = min(max(top, zeroY - (values[index] * _scale)), bottom);
  ```
- Wspólna baza: `DSView/pv/view/trace.h/.cpp` (353+396) — `class Trace : public
  SelectableItem` z wirtualnymi `paint_prepare()`, `paint_back/mid/fore()`,
  `paint_label()`. `DecodeTrace`, `MathTrace`, `LissajousTrace`, `SpectrumTrace`
  dziedziczą `Trace` bezpośrednio (nie przez `Signal`).

### 5. `SRD_OUTPUT_META` — kanał istnieje w rdzeniu C i już częściowo używany przez PWM,
   ale **całkowicie nieobsłużony po stronie C++ DSView**

- `libsigrokdecode4DSL/decoders/pwm/pd.py:66-71` — dekoder PWM **już** rejestruje meta
  output:
  ```python
  self.out_average = self.register(srd.OUTPUT_META,
                        meta=(float, 'Average', 'PWM base (cycle) frequency'))
  ```
  i emituje go (pd.py ~l.140-141):
  ```python
  self.put(self.first_samplenum, self.es_block, self.out_average,
           float(average / num_cycles))
  ```
  Wypełnienie (`percent`) i okres (`period_t`) są dziś wysyłane **tylko** jako tekst
  przez `self.out_ann` (pd.py l.91, l.128) — trzeba by dodać dla nich osobne
  `self.register(OUTPUT_META, meta=(float, ...))` + `self.put(...)` per cykl, analogicznie
  do `self.out_average`.
- Stałe typów wyjścia: `libsigrokdecode.h:133-137`
  ```c
  enum srd_output_type { SRD_OUTPUT_ANN, SRD_OUTPUT_PYTHON, SRD_OUTPUT_BINARY,
                          SRD_OUTPUT_META, ... };
  ```
  eksportowane do Pythona w `module_sigrokdecode.c:60-66`.
- Struktury C (`libsigrokdecode.h`):
  ```c
  struct srd_proto_data {              // linia 342
      uint64_t start_sample;
      uint64_t end_sample;
      struct srd_pd_output *pdo;
      void *data;                       // dla META: GVariant* (int64 albo double)
  };
  struct srd_pd_output {                // linia ~330
      int pdo_id;
      int output_type;
      struct srd_decoder_inst *di;
      char *proto_id;
      const GVariantType *meta_type;    // G_VARIANT_TYPE_INT64 albo _DOUBLE
      char *meta_name;
      char *meta_descr;
  };
  ```
- Konwersja Python → GVariant: `type_decoder.c:414-452`, `convert_meta()` — sprawdza
  `pdata->pdo->meta_type` i robi `g_variant_new_int64()` albo `g_variant_new_double()`.
  Dispatch z `Decoder_put()`: `type_decoder.c:574-586` — woła zarejestrowany callback
  (`cb->cb(&pdata, cb->cb_data)`) **tylko jeśli** ktoś zarobił `srd_pd_output_callback_add`
  dla `SRD_OUTPUT_META` — **nikt dziś tego nie robi**.
- Grep całego `DSView/pv/` za `OUTPUT_META` / `srd_output_type` → **zero wyników**. Jedyna
  rejestracja callbacku to ta na `SRD_OUTPUT_ANN` (`decoderstack.cpp:747-751`, patrz wyżej).
  To jest martwy kanał po stronie GUI — trzeba go dopiero podłączyć.
- Rejestracja wyjścia w Pythonie: `type_decoder.c:606-715`, `Decoder_register()` — tu
  widać że `pdo_id` to po prostu indeks w `di->pd_output` (lista per instancja dekodera),
  a `meta_name`/`meta_descr` to etykiety podane przez autora dekodera — to naturalny
  identyfikator/nazwa wykresu w UI.

### 6. Klasa `Decoder` (C++, per-instancja w stosie dekoderów)

`DSView/pv/data/decode/decoder.h` (133 linii) — `pv::data::decode::Decoder` trzyma
`_decoder` (`srd_decoder*`), `_probes`, `_options`, `_shown` itd. To odpowiednik jednej
pozycji w stosie dekoderów (`DecoderStack::_stack`). Tu (albo w `DecoderStack`) trzeba
będzie dodać strukturę trzymającą per-`(Decoder*, pdo_id)` strumień punktów
`(sample, value)` z meta-outputu.

## Szkic planu implementacji (do zrobienia w WSL)

1. **Python (`libsigrokdecode4DSL/decoders/pwm/pd.py`)** — dodać `OUTPUT_META` dla
   duty-cycle (%) i okresu, analogicznie do istniejącego `self.out_average`. To dekoder
   testowy/referencyjny dla generycznego mechanizmu.
2. **C++ dane** — nowa struktura (np. `pv/data/decode/metasnapshot.h/.cpp` albo rozszerzenie
   `Row`/`Decoder`) trzymająca listę `(start_sample, end_sample, double value)` per
   `(srd_decoder_inst, pdo_id)`, czyli per zarejestrowany meta-output. Wzorować na
   `MathStack` (przechowywanie jako `std::vector<double>` + ewentualna piramida
   min/max do rysowania przy oddaleniu — tu jednak próbki są **nierównomierne w czasie**
   (raz na cykl PWM), więc rysowanie to raczej łamana/schodkowa linia łącząca punkty, nie
   per-sample polyline jak w `AnalogSignal`).
3. **C++ callback** — w `DecoderStack::execute_decode_stack()` (`decoderstack.cpp:747`)
   dodać drugie `srd_pd_output_callback_add(session, SRD_OUTPUT_META,
   DecoderStack::meta_callback, ...)` i nową metodę `DecoderStack::meta_callback()`
   (wzorem `annotation_callback`, `decoderstack.cpp:784-842`) — odczytuje
   `pdata->pdo->meta_type` (int64 vs double), `pdata->data` (`GVariant*`), konwertuje
   przez `g_variant_get_int64`/`g_variant_get_double`, i dopisuje punkt do struktury z
   punktu 2, identyfikowanej przez `pdata->pdo->di` + `pdata->pdo->pdo_id` (ew.
   `meta_name` do etykiety w UI).
4. **C++ rysowanie** — nowa klasa widoku (np. `pv/view/decodemetatrace.h/.cpp`,
   `class DecodeMetaTrace : public Trace`) albo tryb wewnątrz `DecodeTrace` — rysuje
   linię łączącą punkty (z autoskalą min/max jak `MathTrace`/`AnalogSignal`), jako
   dodatkowy wiersz pod dekoderem, włączany/wyłączany per meta-output.
5. **UI** — sposób włączenia trybu wykresu: prawdopodobnie menu kontekstowe na wierszu
   dekodera (por. `pv/widgets/decodermenu.cpp`, `pv/widgets/decodergroupbox.cpp`) z opcją
   "Show as waveform" dla każdego zarejestrowanego meta-output danego dekodera.
6. **Generyczność** — mechanizm ma wykryć *dowolny* zarejestrowany `OUTPUT_META` z
   dowolnego dekodera w stosie (nie tylko PWM) i zaproponować go w UI jako opcjonalny
   wykres — więc kroki 2-5 nie mogą być zahardkodowane pod PWM.

## Otwarte pytania / do ustalenia w kolejnej sesji

- Skalowanie osi Y: auto-fit (min/max widocznego zakresu) czy stała skala z możliwością
  ręcznej regulacji (jak `vDial` w `Signal`)?
- Interpolacja między punktami meta (skoro próbki są nierównomierne w czasie): schodkowa
  (wartość trzymana do następnego punktu — najbliższa semantyce "wypełnienie tego cyklu")
  czy liniowa?
- Czy wykres ma zajmować osobny wiersz (jak `MathTrace`) czy nakładać się na wiersz
  adnotacji tekstowych dekodera (przełącznik trybu w tym samym wierszu)?
- ~~System budowania~~ — **rozwiązane**, patrz niżej.

## Build na WSL (działa)

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build -j$(nproc)
DESTDIR=<gdziekolwiek> cmake --install build --prefix /usr/local
cd <gdziekolwiek>/usr/local/bin && QT_QPA_PLATFORM=xcb ./DSView
```

Cztery pułapki:

1. `CMAKE_POLICY_VERSION_MINIMUM=3.5` — `cmake_minimum_required(VERSION 2.8.6)`
   jest odrzucane przez CMake 4.x twardym błędem.
2. **Nie da się uruchomić binarki z katalogu builda.** `GetAppDataDir()`
   (`DSView/pv/config/appconfig.cpp:461`) szuka tylko `<bindir>/../share/DSView`
   albo `/usr/local/share/DSView`, inaczej `assert(false)` → core dump. Stąd
   install do DESTDIR — pełny layout (res, lang, decoders) bez roota.
3. `QT_QPA_PLATFORM=xcb` — plugin platformy wayland nie jest zainstalowany,
   mimo że WSLg wystawia `WAYLAND_DISPLAY`.
4. Brak pluginu `libqsvg` (`qt6-svg`) — aplikacja startuje, ale ikony SVG są puste.

Zależności obecne w systemie: glib 2.72, zlib, libusb 1.0.25, python3-dev,
libudev, fftw3, boost, Qt 6.2.4. Qt idzie przez CMake config, nie pkg-config.

**Uwaga o CRLF:** repo przyszło z Windows z CRLF, a HEAD ma LF — `git status`
pokazywał 843 "zmodyfikowane" pliki przy zerowej realnej zmianie. Zostało to
znormalizowane (`git checkout -- .`) w sesji 2. Jeśli kiedyś wróci, sprawdzaj
realne zmiany przez `git diff --numstat --ignore-cr-at-eol` (`--name-only`
kłamie, bo porównuje blob, nie treść).

## Kontekst decyzji

Build na Windows (MSVC) jest niestabilny wg zgłoszeń innych kontrybutorów projektu —
nikomu nie udało się go poprawnie zbudować. Dlatego dalsza praca (kompilacja + testowanie
GUI) przenosi się na WSL. Ten plik ma pozwolić wznowić pracę bez powtarzania eksploracji
kodu opisanej wyżej.
