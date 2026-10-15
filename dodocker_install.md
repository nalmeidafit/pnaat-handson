Install Docker on the Raspberry Pi and start your first container.

Update the system
sudo apt update && sudo apt upgrade -y

Run the Docker install script
curl -fsSL https://get.docker.com -o get-docker.sh && sudo sh get-docker.sh

Allow Docker without sudo
sudo usermod -aG docker $USER – then log out and back in

Test the installation
docker run hello-world

Check Docker Compose
docker compose version
