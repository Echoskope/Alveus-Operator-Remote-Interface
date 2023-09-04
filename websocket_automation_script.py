import websocket
import json

# Connect to WebSocket server
ws = websocket.WebSocket()
#ws.connect("ws://192.168.0.26")
ws.connect("ws://esp32.local")
print("Connected to WebSocket server")
loop = True
# Ask the user for some input and transmit it
print("Enter config state for Websocket behavior:")
print("1 - User JSON input only")
print("2 - Websocket receiver only.")
print("3 - Operator Remote Interface test.")

progState = input("Program State: ")

# If publisher sends 999 then publisher and subscribers will all exit

relayState = False

if progState == "1":
    while(1):
        str = input("Say something: ")
        if str == "999":
            ws.send(str)
            break
        else:
            ws.send(str)
elif progState == "2":
    # Wait for server to respond and print it
    while(1):
        result = ws.recv()
        print("Received: " + result)
        if result == "999":
            break
else:
    while(1):    
        result = ws.recv()
        #print("Datatype before deserilization: "+ str(type(result)))

        if result == "999":
            break

        try:
            data = json.loads(result)
            jsonValid = True
        except:
            #print("\nFailed to deserialize: "+str(result)+"\n")
            jsonValid = False

        if jsonValid:
            print("********************************")
            print("********************************")
            print("Data after deserilization: "+str(data))
            buttonEvent = data["event"]["button0"]
            #print(buttonEvent)
            if relayState:
                json_data = '{"control":{"relay0":"off","display0":"Relay Off","redButtonLED":"off","greenButtonLED":"on"}}'
                json_object = json.loads(json_data)
                json_formatted_str = json.dumps(json_object, indent=2)
                print("\nJSON being sent back: ")
                print(json_formatted_str)
                print("\n")
                ws.send(str(json_object))
                relayState = False
            else:
                json_data = '{"control":{"relay0":"on","display0":"Relay On","redButtonLED":"on","greenButtonLED":"off"}}'
                json_object = json.loads(json_data)
                json_formatted_str = json.dumps(json_object, indent=2)
                print("\nJSON being sent back: ")
                print(json_formatted_str)
                print("\n")
                ws.send(str(json_object))
                relayState = True
            



# Gracefully close WebSocket connection
ws.close()