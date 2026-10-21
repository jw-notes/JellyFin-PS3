# Contributing to JellyFin [PS]

Thanks for wanting to help. Bug reports, fixes, translations and new features
are all welcome.

## Reporting a bug

[Open an issue](https://github.com/jw-notes/JellyFin-PS3/issues) and include:

- the app version (Settings → System → About) and your console's firmware;
- your Jellyfin server version, and whether the PS3 is on Wi-Fi or a cable;
- what you did, what you expected, and what happened instead;
- the log, if you can: Settings → System → **Send Log to Server** puts it in
  Jellyfin under Dashboard → Logs (`upload_PS3_….log`), or copy
  `/dev_hdd0/tmp/player_log.txt` over FTP. Remove your server address and
  account name from it before posting if you'd rather keep them private.

## Making a change

1. **Fork** the repo and create a branch for your change.
2. **Build and test** it. [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) covers the
   toolchain (the pinned GCC 7.2 Docker image; newer SDKs freeze on hardware),
   the host tests, and trying a build on a console.
3. **Open a pull request** against `main`, saying what it changes and how you
   tested it (host tests, and which console and firmware if you tried it on
   one).

Nothing reaches `main` without review, and pull requests are usually squashed
into a single commit when merged, credited to you.

## The rules

- **Commits are yours.** Write them under your own name. Don't add
  `Co-authored-by` lines for AI tools or bots, and don't open pull requests
  from bot accounts: the contributors list is for the people who did the work.
  You're welcome to use whatever tools help you, as long as you've read,
  understood and tested what you submit.
- **New on-screen text needs translations.** Every string is in English in the
  source and translated into Japanese, Brazilian Portuguese, German, French and
  Spanish in `source/i18n/catalog/`. Run `python tools/i18n.py gen` and
  `python tools/i18n.py check` before you submit. If you can't translate, say
  so in the pull request and we'll sort it out.
- **Test what can be tested.** Logic that can run on a PC belongs behind a
  host-testable header with a test in `tests/`.
- **Match the code around you**: its style, naming and comment density. Don't
  reformat or rename code you aren't changing.
- **Keep it free.** Only add code, fonts and art under licences compatible with
  the GPLv3, and add them to [CREDITS.md](CREDITS.md). Nothing copied from
  Sony's firmware or SDK.
- **Don't change the release files.** Version numbers, `release/JellyFin-PS3.pkg`
  and the changelog's version headings are updated when a release is made.

## Licence

By contributing you agree that your work is released under the
[GNU GPL, version 3 or later](LICENSE), like the rest of the project.
