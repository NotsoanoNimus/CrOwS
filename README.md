# CrOwS (Cryptography & Ownership System)
A hobby operating system project, allowing full freedom of expression and control over modern hardware, with a focus on security and cryptography at every turn.

This is my tiny test-bed of cool crypto and security concepts.

OSDev-ers _rejoice_, for another teammate has joined your league! Praise the sun! ☀️


## Intended Features & Attributes
- [Microkernel](https://en.wikipedia.org/wiki/Microkernel) and modular design; specific focus on snappy inter-process communications.
- Mostly secure, encrypted, and decoupled _kernel_-to-_initrd_-to-_rootfs_ chain loaded with the [MFTAH file format](https://github.com/NotsoanoNimus/MFTAH).
  - By "secure" I mean the CIA triad is applied to the boot process in a unique way. But CrOwS' boot loader with MFTAH ***does not*** provide AAA.
  - See [MFTAH-MBR (syslinux)](https://github.com/NotsoanoNimus/syslinux) and/or [MFTAH-UEFI](https://github.com/NotsoanoNimus/MFTAH-UEFI) for more details. This means CrOwS is both MBR and UEFI compatible.
- Explicitly _REQUIRED_ software-based full-disk encryption.
  - This is done all the way up through and including the boot image (MBR or UEFI).
  - Since MFTAH can load full virtual disks and provide them to CrOwS, different instances of CrOwS and its snapshots can be preserved as normal (but encrypted) files on any disk, bootable and restorable at any time.
  - Of course, any mounted device outside of the rootfs can be unencrypted, but will ideally use LUKS.
- [Orthogonal persistence](https://en.wikipedia.org/wiki/Persistence_(computer_science)) and [Single-Level Store](https://en.wikipedia.org/wiki/Single-level_store) as built-ins.
  - Related to the above, the user should be able to snapshot the system state at any time (or have an automated process enabled to do so and journal), and these snapshots can be "restored to" by directly flashing them into RAM, particularly upon booting. This is quite idealistic but achievable after extensive work.


## Building
TODO


## Deploying/Running
TODO


## Bugs
Please open an issue or email me directly to report any bugs.

## Miscellaneous
You can find detailed documentation for the project under the [docs folder](/docs/) as they're written.


### Development & Participation
I develop at my own pace using a Rocky Linux 9 machine. Thus, the build system is designed for Linux and I do not plan to support anything else (except maybe my own system if I can get that far).

Crusty Windows bloatware enjoyers are not welcome. I want _nothing_ to do with corporate and corporate sycophants for this project &mdash; FOSS chads only.

If you want to contribute to the project, it's best if you **fork the project, make your changes, and ask me about it**. You can also just fork it and blaze your own path, I don't really care. Just keep the license.

While I have experience in collaborative environments, that's not really my style for this project and I don't really want to deal with all the extra overhead and noise that comes with it. It be like that sometimes.


### The Focus
There are myriad hobby OSes out there; many of them are intriguing, sophisticated, featured, and successful.

I'm not trying to be unique or build the next Linux. My goal for this project is twofold:
1. To curate an extremely challenging, rewarding, and educational hobby that keeps me busy during my downtime, and
2. To build something that people might find interesting or at least partially useful (even as a toy).

In my mind, while I'm trying to avoid being *NIX-like, I sort of conceptualized a Tails+Kali hybrid system but with direct and full control in a different flavor.

~~Truthfully, I skipped plenty of work with boot sectors and system initialization because several educational, secure, and/or featured solutions already exist to do the same thing. I don't have an interest in Legacy BIOS, but I _do_ have an interest in UEFI (though both are supported here).~~

The above statement has been retracted on account of spending the first month of project development on the encrypted image loader for UEFI and Legacy boots. 😎
