# Changes to NimBLE

Patches the nRF52 build applies to its NimBLE checkout (`cmake/hoja_nrf52.cmake`), each one a
`git diff` against the pinned commit.

- `conn-interval-5ms.patch`: the shortest connection interval NimBLE accepts is 5 ms, not the
  7.5 ms the Bluetooth specification sets. A Switch 2 moves its controllers to 5 ms a few seconds
  after connecting; NimBLE's controller ended the link on that update ("Invalid LL Parameters").
