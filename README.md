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
