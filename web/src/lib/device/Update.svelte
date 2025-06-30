
<script lang="ts">
    import {PUBLIC_API_PATH} from "$env/static/public";
    import {apifetch} from "$lib";
    import type {Device} from "$lib"
    import { Button, Dropzone, Img, Progressbar } from "flowbite-svelte";
    import { Modal } from 'flowbite-svelte';
    import { ExclamationCircleOutline } from 'flowbite-svelte-icons';
    
    
    


    type UpdateStatus = {
        state: "running" | "paused" | "complete" | "error" | "none"
        last_started: number
        progress: number
        error: string
        sync: number
    }
    type propsType = {
        selectedDevice: Device | undefined
    }
    let {selectedDevice}: propsType = $props()

    let popupModalStart = $state(false);
    let popupModalError = $state(false);
    let popupModalErrorMessage = $state("")
    let updateStatus: UpdateStatus | undefined = $state()
    let uploadingFile = $state(false)
    //let updateStarted = $state(0)
    let firstFetch = 0
    let file: any =  $state();
    let sync: number = -1
    let fetching: number = 0


    function getUpdateProgress(){
        if(updateStatus){
            if(updateStatus.progress < 0)
                return 0
            else if(updateStatus.progress > 100)
                return 100
            else
                return updateStatus.progress
        }
        return 0
    }
    
    function getLastUpdate(){
        if(updateStatus && updateStatus.last_started > 1000){
            const date = new Date(updateStatus.last_started*1000)
            return date.toLocaleDateString() + " " + date.toLocaleTimeString()
        }
        return ""
    }

    function getStatus(){
        if(updateStatus){
            if(updateStatus.error && updateStatus.error.length > 2)
                return updateStatus.error

            if(updateStatus.state == "running")
                return `updating ${getUpdateProgress()}%`

            if(updateStatus.progress >= 100)
                return "Complete"

        }
        return ""
    }

    function dropHandle(event: Event){
        file = undefined
        event.preventDefault()
        //@ts-ignore
        let items = event.dataTransfer.items
        if (items) {
            [...items].forEach((item, i) => {
                if (item.kind === 'file') {
                    file = item.getAsFile();
                }
            })
        }
        else {
            //@ts-ignore
            [...event.dataTransfer.files].forEach((file, i) => {
                let name = file.name;
                console.log(name)
            })
        }
    }

    function handleChange(event: Event){
        //@ts-ignore
        let files = event.target.files;
        if (files.length > 0) {
            file = files[0]
        }
    }





    async function startUpdate(){

        try {
            console.log(`starting update ${file.name}`)
            uploadingFile = true
            let formData = new FormData();
            formData.append("file", file);
            let resp = await fetch(PUBLIC_API_PATH+`/device/${selectedDevice?.devkey}/update/start`, {
                method: "post",
                body: formData
            })
            let data = await resp.json()    
            if(data.error)
                throw Error(data.error)
            
            if(!data.update){
                console.error(data)
                throw Error("invalid response data")
            }  
            
            updateStatus = data.update
        } catch (err) {
            if(err instanceof Error)
                console.error(`update failed ${err.message}`)
        }

        uploadingFile = false
    }

    async function fetchUpdateStatus(){

        let path = `/device/${selectedDevice?.devkey}/update/status`
        try {
            console.log(`fetching ${path}`)
            fetching = 1
            let data = await apifetch(path, {
                sync: sync
            }) 

            if(!data.update){
                console.error(data)
                throw Error("invalid response data")
            } 

            updateStatus = data.update

            // if(data.update){
            //     updateStatus = data.update as UpdateStatus
            //     // console.log(`updateStarted==${updateStarted} updateStatus.started==${updateStatus.started}`)
            //     if(updateStarted > 0 && updateStatus?.started == updateStarted){
            //         if(updateStatus?.progress < 0 || updateStatus?.progress > 100 || updateStatus?.error.length>0){
            //             updateStarted = 0;
            //             file = undefined
            //         }
            //     }
            
            //     sync = updateStatus.sync
            // }
            // else{
            //     console.error(data)
            //     throw Error("Invalid response data")
            // }
            
        } catch(err) {
            if(err instanceof Error)
                console.error(`fetch ${path} failed ${err.message}`)
        } finally {
            fetching = 0
        }
    }
    

    function onStartButton() {

        if(selectedDevice && selectedDevice.connected){
            if(file){
                let fname = file.name
                if(fname.toLowerCase().endsWith(".bin")){
                    popupModalStart = true
                }
                else{
                    popupModalErrorMessage = "Invalid file type, expected *.bin"
                    popupModalError = true
                }
            }
            else{
                popupModalErrorMessage = "No file selected"
                popupModalError = true
            }
            
        }
        else{
            popupModalErrorMessage = "Device is not connected"
            popupModalError = true
        }
    }

    async function onCancel(){
        try{
            let data = await apifetch(`/device/${selectedDevice?.devkey}/update/cancel`, {

            })

            
        }catch(err){
            if(err instanceof Error)
                console.error(err.message)
        }
    }


    $effect(()=>{

        if(!firstFetch++)
            fetchUpdateStatus()
        
        const interval = setInterval(() => {
            if(fetching==0 && selectedDevice){
                fetchUpdateStatus()
            }
		}, 1000*1)

		return () => clearInterval(interval)
    })

</script>





{#snippet UpdateProgress()}
    <div class="p-28 w-full text-center">
        <div class="mb-1 text-lg font-medium dark:text-white">Updating</div>
        <Progressbar progress={getUpdateProgress()} size="h-4" labelInside  />
    </div>

    <div class="w-full text-end">
        <Button on:click={onCancel} class="me-10">Cancel</Button>
    </div>
{/snippet}

{#snippet _Dropzone()}
    <!-- <Dropzone id="dropzone" on:drop={onDrop} on:dragover={(event) => { event.preventDefault() }} on:change={onChange}>
        <svg aria-hidden="true" class="mb-3 w-10 h-10 text-gray-400" fill="none" stroke="currentColor" viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg">
            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M7 16a4 4 0 01-.88-7.903A5 5 0 1115.9 6L16 6a5 5 0 011 9.9M15 13l-3-3m0 0l-3 3m3-3v12" />
        </svg>
        <p class="mb-2 text-sm text-gray-500 dark:text-gray-400"><span class="font-semibold">Click to select</span> or drag and drop</p>
        <p class="text-xs text-gray-500 dark:text-gray-400">select a file to start the update</p>

    </Dropzone> -->

    <Dropzone id="dropzone" on:drop={dropHandle} on:dragover={(event) => {event.preventDefault();}} on:change={handleChange}>
        <svg aria-hidden="true" class="mb-3 w-10 h-10 text-gray-400" fill="none" stroke="currentColor" viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M7 16a4 4 0 01-.88-7.903A5 5 0 1115.9 6L16 6a5 5 0 011 9.9M15 13l-3-3m0 0l-3 3m3-3v12" /></svg>
        {#if !file}
            <p class="mb-2 text-sm text-gray-500 dark:text-gray-400"><span class="font-semibold">Click to upload</span> or drag and drop</p>
            <p class="text-xs text-gray-500 dark:text-gray-400">Select a file to start the update</p>
        {:else}
            <p>{file.name}</p>
        {/if}
        </Dropzone>

        {#if file}
        <div class="w-full mt-5 text-end">
            <Button on:click={onStartButton} class="">Start update</Button>
        </div>
        {/if}
{/snippet}





<div class="">

        <table class="mb-4">
            <tbody>
                <tr>
                    <td class="font-medium p-1">Last update: </td>
                    <td class="ps-2">{getLastUpdate()}</td>
                </tr>

                <tr>
                    <td class="font-medium p-1">Status: </td>
                    <td class="ps-2 {updateStatus?.error?"text-red-500":"text-base"}"  >{getStatus()}</td>
            </tbody>
        </table>
    

    {#if uploadingFile}
        <div class="p-28 w-full text-center">
            <div class="mb-1 text-lg font-medium dark:text-white">Uploading</div>
        </div>
    {:else if updateStatus && updateStatus.state == "running"}
        {@render UpdateProgress()}
    {:else}
        {@render _Dropzone()}
    {/if}


    <Modal bind:open={popupModalStart} size="xs" autoclose>
    <div class="text-center">
        <ExclamationCircleOutline class="mx-auto mb-4 text-gray-400 w-12 h-12 dark:text-gray-200" />
        <h3 class="mb-5 text-lg font-normal text-gray-500 dark:text-gray-400">Are you sure you want to start the update?</h3>
        <Button color="red" class="me-2" on:click={()=>{startUpdate()}}>Yes, I'm sure</Button>
        <Button color="alternative">No, cancel</Button>
    </div>
    </Modal>

    <Modal bind:open={popupModalError} size="xs" autoclose>
    <div class="text-center">
        <ExclamationCircleOutline class="mx-auto mb-4 text-gray-400 w-12 h-12 dark:text-gray-200" />
        <h3 class="mb-5 text-lg font-normal text-gray-500 dark:text-gray-400">{popupModalErrorMessage}</h3>
        <Button color="alternative">Ok</Button>
    </div>
    </Modal>




</div>