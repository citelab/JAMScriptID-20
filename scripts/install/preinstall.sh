#!/usr/bin/env bash

if [  $(npm -v | cut -d. -f1) -lt "6" ]
then
        echo "npm version older than 6.0.0"
        exit 1
fi


cd deps/mujs2
make
sudo make install
cd ../..

sudo chmod -R go+wx /usr/local/share
sudo chmod go+wx /usr/local/include
sudo chmod -R go+wx /usr/local/lib
sudo chmod go+wx /usr/local/bin

sudo mkdir -p /usr/local/share/jam
sudo chmod go+wx /usr/local/share/jam

mkdir temp_install_src
chmod o+w temp_install_src
cd temp_install_src

if [[ "$OSTYPE" == "linux-gnu"* ]]; then

    echo "-------- Linux detected -----------"
    sudo apt-get update
    sudo apt-get install -y -q xz-utils
    sudo apt-get install -y -q texinfo
    sudo apt-get install -y -q libc-dev
    sudo apt-get install -y -q libhiredis-dev
    sudo apt-get install -y -q libevent-dev
    sudo apt-get install -y -q libbsd-dev
    sudo apt-get install -y -q libssl-dev
    sudo apt-get install -y -q avahi-daemon avahi-discover libnss-mdns libavahi-compat-libdnssd-dev
    sudo apt-get install -y -q unzip

    if (command -v clang > /dev/null); then
        echo "clang already installed.. skipping install"
    else
        sudo apt install -y -q clang
    fi

    if (command -v cmake > /dev/null); then
        echo "cmake already installed.."
    else
        sudo apt install -y -q cmake
    fi

    if (command -v g++ > /dev/null); then
        echo "g++ already installed.."
    else
        sudo apt-get install -y -q g++
    fi

    if (command -v mosquitto > /dev/null); then
        echo "mosquitto already installed.."
    else
        sudo apt-get install -y -q mosquitto
    fi

    if (command -v mosquitto_pub > /dev/null); then
        echo "mosquitto_pub already installed.."
    else
        sudo apt-get install -y -q mosquitto-clients
    fi

    if (command -v tmux > /dev/null); then
        echo "terminal multiplexor already installed.."
    else
        sudo apt-get install -y -q tmux
    fi

    # NANOMSG
    if (command -v nanocat > /dev/null); then
        echo "nanomsg already installed.."
    else
        wget https://github.com/nanomsg/nanomsg/archive/1.0.0.tar.gz
        tar xvzf 1.0.0.tar.gz
        cd nanomsg-1.0.0
        mkdir build
        cd build
        cmake ..
        cmake --build .
        ctest -G Debug
        sudo env "PATH=$PATH" cmake --build . --target install
        sudo ldconfig
        cd ../..
    fi

    echo "Installing.. Cbor... "
    # CBOR
    qres=$(dpkg -s libcbor 2>/dev/null | grep "Status" | tr -d ' ')
    if [ -z $qres ]; then
        wget https://github.com/PJK/libcbor/archive/v0.5.0.zip
        unzip v0.5.0.zip
        cd libcbor-0.5.0
        cmake CMakeLists.txt
        make
        sudo make install
        cd ..
    else
        echo "libcbor already installed..."
    fi

    echo "Installing.. Redis... "
    # Redis
    if (command -v redis-server > /dev/null); then
        echo "Redis already installed"
    else
        wget http://download.redis.io/redis-stable.tar.gz
        tar xvzf redis-stable.tar.gz
        cd redis-stable
        make
        sudo make install
        cd ..
    fi

    echo "Installing.. MQTT... "
    # MQTT
    qres=$(ldconfig -p | grep mqtt3a | tr -d ' ')
    if [ -z $qres ]; then
        wget -O mqtt.tar.gz https://github.com/eclipse/paho.mqtt.c/archive/v1.3.0.tar.gz
        tar xvzf mqtt.tar.gz
        cd paho.mqtt.c-1.3.0
        make
        sudo make install
        cd ..
    fi

elif [[ "$OSTYPE" == "darwin"* ]]; then
    if (command -v ninja > /dev/null); then
        echo "ninja already installed"
    else
        brew install ninja
    fi

    if (command -v mosquitto > /dev/null); then
        echo "mosquitto already installed"
    else
        brew install mosquitto
    fi

    if (command -v cmake > /dev/null); then
        echo "cmake already installed"
    else
        brew install cmake
    fi

    if (command -v tmux > /dev/null); then
        echo "tmux already installed"
    else
        brew install tmux
    fi

    brew install hiredis

    if (command -v nanocat > /dev/null); then
        echo "Nano message already installed"
    else
        brew install nanomsg
    fi

    if (command -v redis-server > /dev/null); then
        echo "Redis already installed"
    else
        brew install redis
    fi

    brew tap pjk/libcbor
    brew install libcbor

    git clone https://github.com/eclipse/paho.mqtt.c.git
    cd paho.mqtt.c
    cmake -GNinja
    ninja
    sudo ninja install
    cd ..
    rm -rf paho.mqtt.c

else
    echo "Unsupported OS"
    exit 1
fi

cd ..

echo "========================================"
echo "Completed the preinstall setup .. $PWD"


sudo rm -rf temp_install_src
