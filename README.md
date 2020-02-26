# webAppServer
This server is used to get the appObjects and generalData for the webApp clients without the need for blockchain or supernode cloud

## Instructions

### To run the server
1. Clone/download this repository
2. Add a strong `<server_password>` in `.config` file
3. Add the `<port>` in `.config` file for the server to run
4. open terminal in the directory and run `./start_webAppServer.sh`

### To use in client side
1. copy the floGlobals and webAppClient script tag to the webApp page
2. Add the server's `<website_url/ip_addr>:<port>` in floGlobals script tag

### Properties

#### sendGeneralData
    webAppClient.sendGeneralData(message, type, options = {})
Sends a message to the cloud via webAppServer
* message: content of the message
* type: type of the message

#### requestGeneralData
    webAppClient.requestGeneralData(type, vectorClock = '0')
Requests the generalData from webAppServer
* type: type of the message
* vectorClock: vectorClock after which the message should be requested (can be used with Date.now()) (default = '0')
* Returns an object (set of generalData)

#### requestObjectData
    webAppClient.requestObjectData(keyPath)
Requests the objectData/appObjects from webAppServer
* path: (an Array) path of the required object (eg. \['Sample', Property1, property2..]
* Returns an Object/Array (requested path)

#### requestSubAdminList
    webAppClient.requestSubAdminList()
Requests the subAdmin list of the application from webAppServer
* Returns an Array (of subAdmins)
