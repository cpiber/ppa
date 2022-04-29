# PPA

This is the source repository for my [ppa](https://launchpad.net/~cppiber/+archive/ubuntu/ppa)

## Building packages

Please refer to `build.md`. Please note that all commands mentioned are expected to be executed in the root directory of this repo, on the corresponding branch.

`pdebuild` will create package-specific files one level up, so it is recommended to clone the repository into a subfolder.

## Upstreams

Below you can find all upstreams in alphabetical order.

```
git remote add cgdb https://github.com/cgdb/cgdb.git
git remote add iverilog https://github.com/steveicarus/iverilog.git
git remote add mpd https://github.com/MusicPlayerDaemon/MPD.git
git remote add ncmpcpp https://github.com/ncmpcpp/ncmpcpp.git
git remote add polybar https://github.com/polybar/polybar.git
git remote add rofi https://github.com/davatorium/rofi.git
git remote add vimpc https://github.com/boysetsfrog/vimpc.git
```

NOTE: None of the source code in other branches is my property. I have only written some of the debian files.
