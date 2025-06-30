

<script lang="ts">


    import { base } from '$app/paths';
    import Header from "$lib/Header.svelte";
    import DeviceList from "$lib/DeviceList.svelte";
    import DeviceControl from "$lib/DeviceControl.svelte";
    import {apifetch } from "$lib"
    import type {Device} from "$lib"
    import {pushState} from "$app/navigation"
    import {onMount} from "svelte"
    import { Button, Modal, Input, Label, Span } from 'flowbite-svelte';
    import NewDeviceDialog from "$lib/NewDeviceDialog.svelte";
    import { dev } from "$app/environment";



    // function createUnivState(def = undefined){
    //     let value = $state(def)
    //     return {
    //         get value() {return value},
    //         set value(newval){ value = newval}
    //     };
    // }

    // let selectedDevice: {
    //     value: Device | undefined
    // } = createUnivState()

    let deviceList: Device[] = $state([])
    let selectedDevice: Device | undefined = $state()
    let newDeviceSelected: Device | undefined = $state()
    let sync = -1
    let once = 0
    let fetching = 0


    async function fetchDeviceList(){

        let path = `/device/list`;
        try {
            fetching = 1
            console.log(`fetching ${path}`)
            let data = await apifetch(path, {
                sync: sync
            })
            if(data.devices){
                deviceList = data.devices
                sync = data.sync
                deviceList.every(device => {
                    if(device.devkey==selectedDevice?.devkey){
                        selectedDevice = device //update data
                        return false
                    }
                })
            }
            else{
                console.error(`fetch ${path} failed, invalid response`)
                console.error(data)
            }
        } catch (err) {
            if(err instanceof Error)
                console.error(`fetch ${path} failed ${err.message}`)
        }
        finally{
            fetching = 0
        }
    }

    
    function onDeviceSelected(device: Device){
      
        selectedDevice = device
        newDeviceSelected = device
        // history.pushState(null, '', '/device/'+device.devkey);    
        // pushState('/device/'+device.devkey, {selectedDevice: device});    
    }



    $effect(()=>{
        const interval = setInterval(() => {
            if(!fetching)
			    fetchDeviceList()
		}, 1000*15)

        if(!once){
            fetchDeviceList()
            once = 1
        }

		return () => clearInterval(interval)
    })


</script>




<div class="flex flex-col h-screen min-w-[1000px] min-h-[800px]">

    <div class="">
        <Header></Header>
    </div>
    

    <div class="flex h-[90%]">

        <div class="w-3/12 p-3 h-[94%]">
            <DeviceList deviceList={deviceList} onSelect={onDeviceSelected} ></DeviceList>        
        </div>

        <div class="w-9/12 p-3 h-[100%]">
            {#key newDeviceSelected}
                <DeviceControl selectedDevice={selectedDevice} ></DeviceControl>
            {/key}
        </div>

    </div>

</div>








<!-- {#await ss}
    loading
{:then res}

{:catch}

{/await} -->



