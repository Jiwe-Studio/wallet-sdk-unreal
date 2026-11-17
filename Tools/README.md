# Tools

Not part of the plugin build. Unreal only compiles what is under `Source/`.

| Folder | What it is |
|---|---|
| `MockJiwe/` | A stand-in for Jiwe that runs on your own computer, for testing without keys. `python mock_jiwe_server.py --port 8765`. See [docs/TESTING.md](../docs/TESTING.md). Copied from the Jiwe Unity SDK tests. |
| `Tests/` | A standalone test of the SHA-256 code the login uses. `g++ -std=c++17 -I ../../Source/JiweSDK/Private sha256_test.cpp -o sha256_test && ./sha256_test` |
