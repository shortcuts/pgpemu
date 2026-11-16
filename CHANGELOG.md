# Changelog

## [1.0.1](https://github.com/shortcuts/pgpemu/compare/v1.0.0...v1.0.1) (2026-02-18)


### Bug Fixes

* npe ([9fbfffb](https://github.com/shortcuts/pgpemu/commit/9fbfffbab83bf2154517dbe80bdd339f179d2ce5))
* remove retoggle and probability feature ([#8](https://github.com/shortcuts/pgpemu/issues/8)) ([d9328e1](https://github.com/shortcuts/pgpemu/commit/d9328e1efb79c4e0522e7a2203f6648d8095b8d0))
* remove retoggle feature ([54ca65a](https://github.com/shortcuts/pgpemu/commit/54ca65a6af464164436e82c00f5b769fb163d04d))
* stalled queue ([7cb1d07](https://github.com/shortcuts/pgpemu/commit/7cb1d07870cb6fcf745b7a82856f40b3413618a5))

## 1.0.0 (2026-02-01)


### Features

* add log levels ([099a1a2](https://github.com/shortcuts/pgpemu/commit/099a1a29549e3dd1f151d191bc517b269c639358))
* add passkey for pairing ([5c324e1](https://github.com/shortcuts/pgpemu/commit/5c324e18ad9b8b4b2fe616fd1997f8c266496202))
* add reset every conns ([4e7a184](https://github.com/shortcuts/pgpemu/commit/4e7a184bfbcbc27ea8a0be129171ed195d8aa30b))
* add spin probability ([ab2bdfc](https://github.com/shortcuts/pgpemu/commit/ab2bdfc15f4c5cbb0c7f2819de4b6d3d2ef32336))
* align with repo example ([b0d5182](https://github.com/shortcuts/pgpemu/commit/b0d5182c03f7ca8cff7604a2e4c7783c91e9213d))
* always restore settings on new connections ([e8918b9](https://github.com/shortcuts/pgpemu/commit/e8918b9ca8cd73f7b05d2c15ba9bd26b2ffbb08a))
* caught, fled and spin counter ([11cac89](https://github.com/shortcuts/pgpemu/commit/11cac89e7132ceaab3c306a3ac670541478aa47f))
* continue on fail write ([5665298](https://github.com/shortcuts/pgpemu/commit/566529830ed931b363f2ceef663a4ccb3f16a383))
* disable spin when bag is full, queue re-enabling ([a0d2081](https://github.com/shortcuts/pgpemu/commit/a0d2081767c5e4b5ff66ecb448cfb09e625ca893))
* enable autospin and autocatch on device connection ([39dbcf3](https://github.com/shortcuts/pgpemu/commit/39dbcf3f9d181b017239ff3565a651403a5fdfb5))
* improve session reconnect ([#7](https://github.com/shortcuts/pgpemu/issues/7)) ([e122906](https://github.com/shortcuts/pgpemu/commit/e12290684eac167557faa9855369bd6410ead054))
* per-device settings, smart box/bag manage, better handshake ([#5](https://github.com/shortcuts/pgpemu/issues/5)) ([1db8713](https://github.com/shortcuts/pgpemu/commit/1db871342eddd933ab7c680e89c052b00729ebf7))
* pgp autosetting to disable when bag/box full ([f5b2dd3](https://github.com/shortcuts/pgpemu/commit/f5b2dd3c60c933479dbaa70aec02f6e50084e5b3))
* reconnect ([7f07132](https://github.com/shortcuts/pgpemu/commit/7f07132fba3bcec2432de43edc406a4a2d808502))
* static secret loading ([026da65](https://github.com/shortcuts/pgpemu/commit/026da6584a1a69c776d07862ecd8c01b2f8a30f3))


### Bug Fixes

* conns and reconns ([62d1488](https://github.com/shortcuts/pgpemu/commit/62d1488aca517b26b1f6b72c7704b79589823ce0))
* ignore binary ([594cca6](https://github.com/shortcuts/pgpemu/commit/594cca69a08b6a9945f3dda67c3e8865328d4b14))
* missing breaks ([c2f6923](https://github.com/shortcuts/pgpemu/commit/c2f69232986aa7b76ca3ef4c8ba86dab7b2512e3))
* monitor cmd ([5f9c21d](https://github.com/shortcuts/pgpemu/commit/5f9c21d411cf244941316b2a0fb093a5a5bf5a5f))
* only toggle if false ([48df0e5](https://github.com/shortcuts/pgpemu/commit/48df0e5134a6b7f654e067b533e5771db1b1b046))
* passkey ([0af1d7d](https://github.com/shortcuts/pgpemu/commit/0af1d7d07a3309e9ca888bed379b4872e0a86def))
* prevent double-incrementing active_connections on reconnection ([8635e22](https://github.com/shortcuts/pgpemu/commit/8635e220960157203e468c0e84f3dcadb76d11cf))
* probability ([7c62590](https://github.com/shortcuts/pgpemu/commit/7c625905ee57f650d49eb3dacb2ace8361638ec7))
* properly track active_connections for reconnections ([197ffed](https://github.com/shortcuts/pgpemu/commit/197ffed4f34b3cdbafebe90766cb53870491b23b))
* readme ([a792c61](https://github.com/shortcuts/pgpemu/commit/a792c61b81c069708cefc2f65e43d19bce388979))
* Remove explicit encryption request to avoid passkey prompts ([b9e0cf8](https://github.com/shortcuts/pgpemu/commit/b9e0cf8e8798c48e39052f45d6c5b023ea17dea7))
* Remove MITM requirement for encryption to avoid passkey prompts on reconnect ([49d25a5](https://github.com/shortcuts/pgpemu/commit/49d25a5ea74d6bea836d6c28390437927bcc0cc5))
* remove wifi portal ([372086d](https://github.com/shortcuts/pgpemu/commit/372086d566554e48dcbf01d1a3101370ffeb0200))
* Replace snprintf hex formatting with manual conversion ([cf2f355](https://github.com/shortcuts/pgpemu/commit/cf2f355e610c40ca3f26944d35f54766feae9e13))
* Resolve critical bugs and improve code safety ([a842835](https://github.com/shortcuts/pgpemu/commit/a842835b506bcd6338088c97e29562d1388d38db))
* restart advertising after reconnection completes ([4e269dd](https://github.com/shortcuts/pgpemu/commit/4e269ddbffca9685bdd1113d80bb1995b968af1c))
* retoggle of settings ([e532570](https://github.com/shortcuts/pgpemu/commit/e53257022680436b6d75da50a5196916890309a3))
* sdkconfig ([2dacfe8](https://github.com/shortcuts/pgpemu/commit/2dacfe84d6878d0117ce33abee2da3ca5edf957b))
