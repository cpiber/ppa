# Building the package

## create persistent build environment (only once)

`sudo pbuilder create --debootstrapopts --variant=buildd --distribution <series> --basetgz ../<series>.tgz` (e.g. `focal`)

## update above environment

Be sure to do this every once in a while. If you need new sources (in `/etc/apt/sources.list`), be sure to specify them here with `--othermirror` (see `man pbuilder`)

`sudo pbuilder update --basetgz ../<series>.tgz`

Launchpad.net adds your own ppa, the main ppa of the series, security and updates.

## create orig tar and init debian (already done in this repo)

`dh_make --createorig -p <name>_<version>`

## build

`pdebuild -- --basetgz ../<series>.tgz` (Might need to `cp` orig)

deb will be in `/var/cache/pbuilder/result`, all other files will have a copy in parent directory


# Uploading to launchpad

## sign and upload

`debsign <changesfile>`

`dput ppa:<user>/<ppa> <changesfile>` ([Uploading](https://help.launchpad.net/Packaging/PPA/Uploading))
