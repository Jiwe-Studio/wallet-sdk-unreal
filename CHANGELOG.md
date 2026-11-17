# Changelog

## 0.1.0 (preview) - 16 November 2026

First build. A port of the Jiwe Unity SDK (`wallet-sdk-unity`) to an Unreal Engine plugin.

### Included

- One `Jiwe Subsystem` for everything, set up from Project Settings > Plugins > Jiwe SDK.
- Login (OAuth 2 Authorization Code + PKCE) on Windows, Mac and Linux through a loopback
  redirect, and on Android through a custom-scheme deep link (not yet device-tested).
- Wallet calls: XP and airtime rewards, leaderboard, wallet balance, rewards earned (API key
  and bearer), purchases with idempotency key, purchase and transaction status, user data,
  game data, credential check.
- Blueprint nodes with On Success / On Failure pins for every call, and C++ callbacks.
- Console commands: `Jiwe.Status`, `Jiwe.Login`, `Jiwe.Cancel`, `Jiwe.Logout`, `Jiwe.GiveXp`,
  `Jiwe.Leaderboard`, `Jiwe.Balance`, `Jiwe.CheckKeys`.
- The mock Jiwe server from the Unity SDK tests, for testing without keys.
- MIT licence.

### Deliberate differences from the Unity SDK

- One object instead of two components; one settings page with the two key pairs in
  separate groups.
- The desktop loopback port is fixed (53682) by default. Unity defaults to a random port,
  which can never match a registered Redirect URI.
- The time allowed to finish logging in in the browser (180 s) is separate from the network
  timeout (20 s). Unity uses one 20 s value for both.
- Login On Start is off by default.
- Leaderboard rows expose a single `Score`, are sorted by rank, and do not expose other
  players' `email` or `phone_number`.
- A reward of 0 or less XP is refused locally with an explanation, before any request.
- A failed or cancelled login never leaves tokens behind; tokens are stored only when the
  whole login succeeds.
- `Logout` added.
- Error messages name the setting or the Redirect URI to fix.

### Not included yet

- Web login (Unreal Engine 5 has no web export from Epic).
- iOS login.
- Sample Blueprint assets.
