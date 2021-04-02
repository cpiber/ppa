# Building the package

## create persistent build environment (only once)

`sudo pbuilder create --debootstrapopts --variant=buildd --distribution <series> --basetgz ../<series>.tgz` (e.g. `focal`)

## create orig tar and init debian (already done in this repo)

`dh_make --createorig -p <name>_<version>`

## build

`pdebuild -- --basetgz ../<series>.tgz` (Might need to `cp` orig)

deb will be in `/var/cache/pbuilder/result`


# Uploading to launchpad

## sign and upload

`debsign <changesfile>`
`dput ppa:<user>/<ppa> <changesfile>` ([Uploading](https://help.launchpad.net/Packaging/PPA/Uploading))
