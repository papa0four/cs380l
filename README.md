# cs380l
repo for advanced operating systems UT Austin

## to add an SSH key for push/pull operations perform the following:
  1. open a terminal
  2. execute the following replacing the example email with your GitHub email
  ```bash
  ssh-keygen -t ed25519 -C "your_email@example.com"
  ```
  3. when prompted to enter a file name, just hit **Enter**
  4. when prompted to enter a passphrase, hit **Enter** twice
  5. view and copy public key to file for easy import to GitHub
  ```bash
  cat ~/.ssh/id_ed25519.pub > key.txt
  ```
  6. if you have VSCode set up with SSH-remote and are connected to your guest OVA, this file will appear within your file list on your host VSCode instance
  7. open *key.txt*, copy the entire contents, and paste it into GitHub SSH key generator

## clone cs380l repo to local dev environment
  1. to initialize git within VSCode, open a terminal *ctrl+shift+`*, and type the following
     ```bash
     git init
     ```
  2. clone **cs380l** repo to your guest environment by selecting the *code* dropdown and clicking the *SSH* option
  3. copy the provided url and perform the following within the VSCode terminal:
     ```bash
     git clone git@github.com:your_username/cs380l.git
     ```
  4. when making updates to your local wish.h/c files you can copy them into the *cs380l* directory before pushing to your branch
  5. If you wish to only maintain one local copy of the code at all times, simply move all relevant files and directories to the cs380l directory
     - **NOTE** if you choose this option, ensure your *Makefile* and all test directories are set up to handle the new file structure for building and testing
  6. Once cloned, ensure all relevant files exist within your local VSCode environment and dev away!