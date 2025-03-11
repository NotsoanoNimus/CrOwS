# Project Dependencies
Any submodule listed here is a CrOwS dependency that is referenced from more than two separate subdirectories of the project.

Many `Makefile` INCLUDE directories point to this `deps` folder to source necessary dependencies at build-time.

There are some submodules which are not listed here because they are specially defined for use in specific modules. For example, `SYSLINUX` is used for `mbr` installations of CrOwS, but it's not in this folder because it's only needed under the `src/boot/mbr` area during particular builds.

Even though `gnu-efi` is only used in `uefi` builds, it's referenced in multiple places throughout the project. So it goes here.
