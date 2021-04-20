# Datacenter GPU Manager (DGM)

DGM is a tool for easier management of Intel GPU in datacenters.

With DGM, you can :
- get telemeteries like instant power and frequencies, and some health info of GPU
- set power or frequency capping easily
- get the knowledge of GPU inventory

# Achitecture

DGM is designed in Server/Client mode. In nodes that runs DGM, server is a deamon process that monitories GPU status, and persists telemetried data into db, and listens on a port and responds to client request by http protocol. Client - dgmcli is a command line tool that you can use it to intereact with server deamon.