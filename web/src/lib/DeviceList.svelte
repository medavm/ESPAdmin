
<script lang="ts">

    import { apifetch } from "$lib";
    import type {Device} from "$lib"
    import { Button, Modal, Input, Label, Span,  } from 'flowbite-svelte';
    import NewDeviceDialog from "$lib/NewDeviceDialog.svelte";
    import { ExclamationCircleOutline } from 'flowbite-svelte-icons';
   

    // let deleteModal = $state(false)
    // let devices: Device[] = $state([])
    // let sync = -1
    
    // type propsType={
    //     selectedDevice: Device | undefined
    // }
    // let {selectedDevice}: propsType  = $props()

    // $effect(()=>{
    //     if(devices.length && !selectedDevice.value)
    //         selectedDevice.value = devices[0]
    //     if(devices.length){
    //         if(selectedDevice.value){
    //             let k = selectedDevice.value.devkey
    //             devices.every(device => {
    //                 if(device.devkey==k){
    //                     selectedDevice.value = device
    //                     return false
    //                 }
    //             });
    //         }
    //         else{
    //             selectedDevice.value = devices[0]
    //         }
    //     }
    // })
    
    
    // $effect(()=>{
    //     fetchDevices()
    // })

    // $effect(()=>{
    //     const interval = setInterval(() => {
	// 		fetchDevices()
	// 	}, 1000*30)

	// 	return () => clearInterval(interval)
    // })

    


    // async function fetchDevices(){
    //     try {
    //         console.log("fetching /device/list")
    //         let data = await apifetch("/device/list", {sync: sync})
    //         if(data.devices){
    //             devices = data.devices
    //             sync = data.sync
    //         }
    //         else{
    //             console.error("fetch /device/list missing 'devices'")
    //         }
    //     } catch (err) {
    //         if(err instanceof Error)
    //             console.error(err.message)
    //     }
       
    // }
    

    let deleteModal = $state(false)
    let selectedDevice: Device | undefined = $state()
    
    type propsType={
        deviceList: Device[]
        onSelect: (device: Device) => any
    }
    let {deviceList, onSelect}: propsType  = $props()


    async function onDelete(){

        let path = `/device/${selectedDevice?.devkey}/remove` 
        try{
            console.log(`fetching ${path}`)
            let resp = await apifetch(path, {

            })
            location.reload()
            
        }catch(err){
            if(err instanceof Error)
                console.error(`fetch ${path} failed ${err.message}`)
        }
    }

    function indicatorColor(device: Device){
        if(device.connected)
        {
            if(device.rssi && device.rssi > -80)
                return "bg-green-400"   
            else
                return "bg-yellow-400"
        }
        else
        {
            return "bg-red-400"
        }
    }

    function onDeviceSelect(device: Device){
        selectedDevice = device
        onSelect(device)
    }

    $effect(()=>{
        if(!selectedDevice && deviceList.length>0){
            onDeviceSelect(deviceList[0])
        }
    })

</script>


{#snippet deleteDevice()}
<Button size="xs" on:click={()=>deleteModal=true} >Delete</Button>
<Modal bind:open={deleteModal} size="xs" autoclose>
    <div class="text-center">
      <ExclamationCircleOutline class="mx-auto mb-4 text-gray-400 w-12 h-12 dark:text-gray-200" />
      <h3 class="mb-5 text-lg font-normal text-gray-500 dark:text-gray-400">Are you sure you want to delete device '{selectedDevice?.name}'?</h3>
      <Button color="red" class="me-2" on:click={onDelete}>Yes, I'm sure</Button>
      <Button color="alternative">No, cancel</Button>
    </div>
</Modal>
{/snippet}



<div class="w-full inline-block border-b-4 p-1">
    <Span class="float-start" >Devices</Span>
    <div class="float-end">
        <NewDeviceDialog>New</NewDeviceDialog>
        {@render deleteDevice()}
    </div>
</div>

<div class="h-full overflow-x-hidden overflow-y-scroll">  
    <ul>
        {#each deviceList as device}
            <li class="w-full p-1 mb-1 border-b-2 cursor-pointer hover:bg-gray-100 {device.devkey==selectedDevice?.devkey?"bg-gray-100":"bg-none"}">
                <button class="w-full text-left" onclick={e => {onDeviceSelect(device)}}>
                    <div class="">
                        <span class="text-xs font-medium overflow-hidden">{device.name}</span>
                    </div>
                    <div>
                        <div class="inline-block w-[10px] h-[10px] rounded-full {indicatorColor(device)} "></div>
                            <div class="inline-block">
                                <span class="text-[10px] font-normal">RSSI: </span>
                                <span class="text-[10px] font-light">{device.rssi}</span>
                            </div>
                    </div>
                </button>
                
            </li>
        {/each}
    </ul>
</div>









