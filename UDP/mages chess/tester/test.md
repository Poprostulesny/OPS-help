# Testerka do L8: Mages' Chess

## Pliki

- `tester/test_mages.py` - czarnopudelkowy tester w Pythonie 3.
- Testowany program domyslnie: `./sop-mag` w katalogu zadania.
- Tester uruchamia serwer UDP na losowym wolnym porcie, wysyla pakiety 16-bajtowe i sprawdza stdout oraz wybrane odpowiedzi UDP.

## Format pakietow uzywany przez tester

Kazda poprawna wiadomosc ma dokladnie 16 bajtow:

- bajt `0`: typ wiadomosci: `l`, `c` albo `q`,
- bajt `1`: padding, zawsze `\0`,
- bajty `2..15`: cialo wiadomosci dopelnione zerami.

Login:

- typ `l`,
- cialo: nazwa gracza, maksymalnie 14 bajtow, dopelniona `\0`.

Cast:

- typ `c`,
- cialo: trzy liczby `uint16_t`: numer zaklecia, `x`, `y`,
- domyslnie tester wysyla liczby w network byte order, czyli big endian.

Quit:

- typ `q`,
- puste cialo.

Jesli Twoj serwer interpretuje `uint16_t` w kolejnosci lokalnej typowej dla x86_64, uruchom tester z `--endian little`.

## Uruchamianie

Wejdz do katalogu zadania:

```bash
cd "UDP/mages chess"
```

Odpal wszystko:

```bash
python3 tester/test_mages.py
```

Konkretny etap:

```bash
python3 tester/test_mages.py --stage stage1
python3 tester/test_mages.py --stage validation
python3 tester/test_mages.py --stage stage2
python3 tester/test_mages.py --stage stage3
python3 tester/test_mages.py --stage stage4
```

Z wypisaniem przechwyconego stdout serwera przy zaliczonych testach:

```bash
python3 tester/test_mages.py --verbose
```

Z innym binarnym programem:

```bash
python3 tester/test_mages.py --binary ./sop-mages
```

Bez automatycznego `make`, gdy binarka juz istnieje:

```bash
python3 tester/test_mages.py --no-build
```

## Co sprawdzaja scenariusze

`stage1`

- wysyla 4 poprawne wiadomosci,
- oczekuje loginu, castu, quit,
- oczekuje zakonczenia programu po 4 poprawnych wiadomosciach.

`validation`

- wysyla zly rozmiar datagramu,
- wysyla zly typ wiadomosci,
- wysyla zly numer zaklecia,
- wysyla zle wspolrzedne,
- potem wysyla poprawny login i sprawdza, czy serwer dalej dziala.

Ten test nie wymusza konkretnej tresci komunikatow bledow, bo w zadaniu jest tylko "appropriate error message".

`stage2`

- loguje dwoch graczy, zeby bylo kompatybilne takze z etapami 3-4,
- wysyla kilka castow,
- oczekuje, ze po opoznieniu familiara pojawia sie komunikaty castowania.

`stage3`

- sprawdza odrzucanie castu przed startem gry,
- loguje dwoch graczy z roznych portow UDP,
- wysyla cast od trzeciego, niezalogowanego klienta,
- wysyla `quit` od drugiego gracza,
- oczekuje komunikatu poddania i zwyciestwa pierwszego gracza.

`stage4`

- loguje dwoch graczy,
- pierwszy gracz rzuca `Summon Elemental` na pole `2,2`,
- drugi gracz rzuca `Divination` na `2,2`,
- oczekuje 50-bajtowej odpowiedzi UDP,
- sprawdza, czy srodkowe pole widziane przez drugiego gracza ma wartosc `2`, czyli elemental przeciwnika.

## Interpretacja wyniku

Przyklad:

```text
[PASS] validation
[FAIL] stage3: stage3: server did not exit after logged-in player quit
```

`PASS` znaczy, ze scenariusz przeszedl.

`FAIL` znaczy, ze program dzialal, ale zachowanie nie pasowalo do oczekiwan.

`ERROR` zwykle oznacza problem techniczny testera, uruchomienia programu albo nietypowe przerwanie procesu.

## Uwagi

- Tester jest pomocniczy, nie jest oficjalna wyrocznia do punktacji.
- Scenariusze dla pozniejszych etapow sa pisane tak, zeby nie przeszkadzaly poprawnemu rozwiazaniu etapow 3-4.
- Dla etapu 1 osobny test `stage1` oczekuje dokladnie zachowania z tresci, czyli zakonczenia po 4 obsluzonych wiadomosciach.
- Jezeli testy castow nie przechodza, a loginy dzialaja, sprawdz najpierw endianowosc liczb `uint16_t`: domyslne `--endian big` kontra `--endian little`.
