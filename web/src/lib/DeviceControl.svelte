<script lang="ts">
    import { Tabs, TabItem } from 'flowbite-svelte';
    import DeviceInfo from './device/DeviceInfo.svelte';
    import type {Device} from "$lib"
    import Update from './device/Update.svelte';
    import Logs from './device/Logs.svelte';
    import Restart from './device/Restart.svelte';



    type propsType = {
        selectedDevice: Device | undefined
    }

    let {selectedDevice}: propsType = $props()





  </script>


<div class="h-full ">


    <Tabs tabStyle="underline">

        <TabItem open title="Info" >
            <DeviceInfo device={selectedDevice}></DeviceInfo>
                <!-- <span>test</span> -->
        </TabItem>

        <TabItem title="Live Data">
            <div class=" w-full text-center">
                <p class="text-sm text-gray-500 dark:text-gray-400">Not Implemented</p>
            </div>
        </TabItem>
        <TabItem title="Settings">
            <div class=" w-full text-center">
                <p class="text-sm text-gray-500 dark:text-gray-400">Not Implemented</p>
            </div>
        </TabItem>
        <TabItem title="Logs">
            <Logs selectedDevice={selectedDevice}></Logs>
        </TabItem>
        <TabItem title="Update">
            <Update selectedDevice={selectedDevice}></Update>
        </TabItem>
        <TabItem title="Restart">
            <Restart selectedDevice={selectedDevice}></Restart>
        </TabItem>

        <!-- <TabItem title="dawd" disabled>
            <p class="text-sm text-gray-500 dark:text-gray-400">
            <b>Disabled:</b>
            Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.
            </p>
        </TabItem> -->
    </Tabs>
    
    
</div>
  
