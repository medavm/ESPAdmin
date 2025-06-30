

<script>
    import { Input, Button, Modal, Span, Alert } from "flowbite-svelte";

    import {apifetch} from "$lib"


    let openDialog = false
    
    let errorMsg = ""
    let name = ""
    let createDis = false
    
    function onType(){
        if(name.length)
            createDis=false
        else
            createDis= true
    }

    function onClose(){
        errorMsg = ""
        name = ""
        openDialog = false
    }
    
    
    async function onCreate(){
        
        try{
            let data = await apifetch("/device/create", {
                devname: name,
            })

            console.log(data)

            name = ""
            openDialog = false

            location.reload()

        }catch(err){
            if (err instanceof Error) 
                errorMsg = err.message
        }        
        
    }


    
</script>




<Button size="xs"  on:click={() => (openDialog = true)}><slot></slot></Button>
<Modal title="Enter new device name" dismissable={false} bind:open={openDialog} autoclose={false}>
    <Alert class="{errorMsg?"":"hidden"}">
        <span class="font-medium">Error</span><br>
        {errorMsg}
    </Alert>
    <Input on:input={onType} bind:value={name} id="device_name" name="device_name" required placeholder="ESP32 example" />
    <Button disabled={createDis} on:click={onCreate}>Create</Button>
    <Button color="alternative" on:click={onClose}>Cancel</Button>
</Modal>

