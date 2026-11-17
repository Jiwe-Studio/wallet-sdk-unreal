# First test: checklist for the tester

Thank you for testing. This is the first time the plugin meets a real Unreal Engine
build, so **the build itself is the first test**. Please note what happens at each step
and send the table at the bottom back.

You need: Unreal Engine 5.3 or newer, a C++ toolchain (Visual Studio 2022 on Windows),
and Python 3 for Part B. About 30 minutes.

---

## Part A: Build

1. Use any C++ project (the Third Person template with C++ is fine). Close the editor.
2. Copy the plugin folder to `YourProject/Plugins/JiweSDK/`.
3. Right-click the `.uproject` > **Generate Visual Studio project files**.
4. Open the `.uproject`. When asked to rebuild **JiweSDK**, choose **Yes**.

**If the build fails:** that is useful. Please send the **first** error lines (from
`Saved/Logs/` or the Visual Studio Output window), your engine version and your OS. Do
not spend time fixing it yourself.

**If it builds:** open **Edit > Plugins**, search "Jiwe", confirm **Jiwe SDK** is ticked.
Open **Edit > Project Settings > Plugins > Jiwe SDK** and confirm you see six numbered
groups.

---

## Part B: Test without Jiwe keys (mock server)

The mock pretends to be Jiwe on your own computer. No account needed.

1. Start it. In a terminal, from the plugin folder:

   ```
   cd Tools/MockJiwe
   python mock_jiwe_server.py --port 8765
   ```

   It prints `mock jiwe on http://127.0.0.1:8765`. Leave it running.

2. Point the plugin at it. Either type these into the settings page, or close the editor
   and paste this block into `YourProject/Config/DefaultGame.ini`:

   ```ini
   [/Script/JiweSDK.JiweSettings]
   ClientId=client-1
   ApiSecret=secret-1
   ApiUsername=test-user
   ApiKey=test-key
   LoopbackPort=53682
   AuthorizationEndpoint=http://127.0.0.1:8765/auth
   TokenEndpoint=http://127.0.0.1:8765/token
   UserInfoEndpoint=http://127.0.0.1:8765/me
   WalletBaseUrl=http://127.0.0.1:8765/rest
   ```

3. Press **Play**. Click in the game window, press the backtick key (`` ` ``) and type
   each command. Results show on screen and in the Output Log (filter: `LogJiwe`).

| # | Type this | You should see |
|---|---|---|
| B1 | `Jiwe.Status` | Four `[OK]` lines, "TEST environment", Redirect URI `http://127.0.0.1:53682/` |
| B2 | `Jiwe.Login` | A browser tab opens and says **Login complete**. In game: `logged in. name=Test Player nickname=tester id=player-123` |
| B3 | `Jiwe.GiveXp 10` | `10 XP awarded. transaction=` followed by a long ID |
| B4 | `Jiwe.GiveXp 0` | `XP not awarded. XP must be greater than 0 (got 0)...` |
| B5 | `Jiwe.Leaderboard` | `#1 Ann 1641` then `#2 Max 915` |
| B6 | `Jiwe.Leaderboard xp week` | `#1 Ann 900`, `#2 Bob 300`, `#3 Cy 500` |
| B7 | `Jiwe.Balance` | `available=150.50 (credits=250.50, debits=100.00)` |
| B8 | `Jiwe.CheckKeys` | `Wallet keys were not rejected` |
| B9 | `Jiwe.Logout` then `Jiwe.GiveXp 10` | `No player is logged in...` |
| B10 | Change **API Key** to `wrong`, then `Jiwe.Balance` | `Invalid username/api key` and a hint to check the Wallet keys. Put `test-key` back after |
| B11 | Stop the mock (Ctrl+C), then `Jiwe.Balance` | `No answer from Jiwe (timeout or no internet connection)` within about 20 seconds |
| B12 | Start the mock again. `Jiwe.Login`, wait for it to finish, then `Jiwe.Login` once more | It works a second time: another tab, the same success line |

B5 and B6 matter more than they look: they check that the three different score fields
Jiwe uses (`XP`, `currentXP`, `xp`) all land in one `Score`.

---

## Part C: Blueprint check (5 minutes)

1. Open the Level Blueprint.
2. From **Event BeginPlay**, right-click and type **Jiwe**. You should see the Jiwe nodes.
3. Add **Jiwe Get Leaderboard** (Reward Type `xp`, Max Entries `10`, Period empty).
4. From **On Success**, add a **For Each Loop** over **Entries** and **Print String** the
   **Name**. From **On Failure**, **Print String** the **Error**.
5. Press Play with the mock running. Ann and Max should print.

---

## Part D: Live Jiwe (only if Jiwe gave you keys)

1. In the settings page, **clear all four fields in group 6 - Test Environment**.
2. Paste the four keys from Jiwe into groups 1 and 2.
3. Confirm with Jiwe that `http://127.0.0.1:53682/` is registered as a Redirect URI on
   that app.
4. `Jiwe.Status` should now say "Live Jiwe".
5. `Jiwe.Login` (sign in with a real Jiwe account), `Jiwe.GiveXp 1`, `Jiwe.Leaderboard`,
   `Jiwe.Balance`.

---

## Part E: Android (optional)

1. Set **Android Redirect Scheme** to a lowercase name, for example `jiwetest`.
2. Register `jiwetest://oauth-callback` on the Jiwe app (live keys needed: a phone cannot
   reach a mock on your PC at 127.0.0.1).
3. Package for Android and install.
4. Add a button that calls **Jiwe Login**, tap it, sign in. The game should come back to
   the front and **On Login Succeeded** should fire.

If the build log from packaging mentions `JiweSDK` and an error, please send it.

---

## Send this back

| Step | Result (pass / fail / skipped) | Notes |
|---|---|---|
| Engine version and OS | | |
| A: build | | |
| A: plugin and settings page visible | | |
| B1 to B12 (list any that failed) | | |
| C: Blueprint nodes | | |
| D: live Jiwe | | |
| E: Android | | |
| Anything confusing in the README | | |

The last row is as important as the others. If any instruction made you stop and think,
say which one.
