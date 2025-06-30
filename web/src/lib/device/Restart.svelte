


<script lang="ts">
    import { apifetch, type Device } from "$lib";
    import { Button, Modal } from "flowbite-svelte";
    import { ExclamationCircleOutline } from "flowbite-svelte-icons";


    type propsType = {
        selectedDevice: Device | undefined
    }
    let {selectedDevice}: propsType = $props()
    let popupModalSure = $state(false)
    let popupModalSureMessage = $state("")
    let popupModal= $state(false)
    let popupModalMessage = $state("")
    let fetching = 0
    let disableButton = $state(false)

    async function requestRestart(){

        let path = `/device/${selectedDevice?.devkey}/restart`
        disableButton = true
        try{
            console.log(`fetching ${path}`)
            fetching = 1
            let data = await apifetch(path, {
            })

            if(data && data.result){
                popupModalMessage = "Device is restarting"
                popupModal = true
            } else {
                console.error(data)
                popupModalMessage = "Device restart failed"
                popupModal = true
                disableButton = false
            }  
        }catch(err){
            if(err instanceof Error)
                console.error(`fetch ${path} failed ${err.message}`)
        }
        finally{
            fetching = 0
        }
    }


    function startRestart(){

        if(!selectedDevice)
            return
        if(!selectedDevice.connected){
            popupModalMessage = "Device is not connected"
            popupModal = true
            return
        }

        if(!fetching){
            requestRestart()
        }

    }

    function onRestartButton(){
        popupModalSureMessage ="Are you sure?"
        popupModalSure = true
    }


</script>





<div class="h-[300px] flex flex-col justify-center items-center">
    <Button disabled={disableButton} class="w-fit" size="xl" on:click={onRestartButton}>Restart device</Button>
</div>


<Modal bind:open={popupModalSure} size="xs" autoclose>
    <div class="text-center">
        <ExclamationCircleOutline class="mx-auto mb-4 text-gray-400 w-12 h-12 dark:text-gray-200" />
        <h3 class="mb-5 text-lg font-normal text-gray-500 dark:text-gray-400">{popupModalSureMessage}</h3>
        <Button color="red" class="me-2" on:click={startRestart}>Yes, I'm sure</Button>
        <Button color="alternative">No, cancel</Button>
    </div>
</Modal>


<Modal bind:open={popupModal} size="xs" autoclose>
    <div class="text-center">
        <ExclamationCircleOutline class="mx-auto mb-4 text-gray-400 w-12 h-12 dark:text-gray-200" />
        <h3 class="mb-5 text-lg font-normal text-gray-500 dark:text-gray-400">{popupModalMessage}</h3>
        <Button color="alternative">Ok</Button>
    </div>
</Modal>