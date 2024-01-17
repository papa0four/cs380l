# cs380l
repo for advanced operating systems UT Austin

## to add an SSH key for push/pull operations perform the following:
<ol>
  <li>open a terminal</li>
  <li>execute the following replacing the example email with your GitHub email</li>
  ```bash
  ssh-keygen -t ed25519 -C "your_email@example.com"
  ```
  <li>when prompted to enter a file name, just hit **Enter**</li>
  <li>when prompted to enter a passphrase, hit **Enter** twice</li>
  <li>view and copy public key to file for easy import to GitHub</li>
  ```bash
  cat ~/.ssh/id_ed25519.pub > key.txt
  ```
  <li>if you have VSCode set up with SSH-remote and are connected to your guest OVA, this file will appear within your file list on your host VSCode instance</li>
  <li>open *key.txt*, copy the entire contents, and paste it into GitHub SSH key generator</li>
</ol>

