
<script lang="ts">
    import { dev } from '$app/environment';




    import {apifetch} from '$lib';
    import type {Device} from "$lib"

    type propsType = {
        device: Device | undefined
    }
    let {device}: propsType = $props()

    // type DeviceInfo = {
    //     name: string
    //     connected: number
    //     rssi: number
    //     lastconnect: number
    //     lastdisconnt: number
    //     devkey: string
    // }

    // let deviceInfo = device
    // let deviceInfo: DeviceInfo | undefined = $state()
    // let sync = -1

    


    // $effect(()=>{
    //     if(device)
    //         fetchData()
    // })

    // async function fetchData(){
    //     try{
    //         console.log(`fetching /device/${device?.devkey}/info`)
    //         let data = await apifetch(`/device/${device?.devkey}/info`, {sync: sync})
    //         deviceInfo = data
    //     } catch (err) {
    //         if(err instanceof Error)
    //             console.error(err.message)
    //     }
    // }

    function getTime(stamp: number | undefined){
        if(stamp){
            const date = new Date(stamp*1000)
            return date.toLocaleDateString() + " " + date.toLocaleTimeString()
        }
        return ""
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



</script>



{#if device}

    <div class="">


        <table>
            <tbody>
                <tr>
                    <td class="font-medium p-1">Device name: </td>
                    <td class="ps-2">{device.name}</td>
                </tr>

                <tr>
                    <td class="font-medium p-1">Connected: </td>
                    <td class="ps-2">
                        <div class="inline-block w-[10px] h-[10px] rounded-full {indicatorColor(device)} ml-1"></div>
                    </td>
                </tr>

                <tr>
                    <td class="font-medium p-1">RSSI: </td>
                    <td class="ps-2">{device.rssi}</td>
                </tr>

                <tr>
                    <td class="font-medium p-1">24h reconnects: </td>
                    <td class="ps-2">{device.connections_24h}</td>
                </tr>

                <tr>
                    <td class="font-medium p-1">Last connect: </td>
                    <td class="ps-2">{getTime(device.last_connect)}</td>
                </tr>

                <tr>
                    <td class="font-medium p-1">Last disconnect: </td>
                    <td class="ps-2">{getTime(device.last_disconnect)}</td>
                </tr>

                <tr>
                    <td class="font-medium p-1">Device key: </td>
                    <td class="ps-2">{device.devkey}</td>
                </tr>
            </tbody>
        </table>

    </div>

{/if}
