


<script lang="ts">

    import { base } from '$app/paths';
    import {apifetch, saveSession} from "$lib"
    import {onMount} from "svelte"

    let email = ""
    let passw = ""
    let rember = false
    let err = ""

    onMount(async ()=>{
        try{
  
            let data = await apifetch("/device/list", {sync: -1})
            console.log("already logged in? redirecting to /home")
            console.log(data)
            window.location.href = "./home"
        
        }
        catch(err){

        }
    })

    async function onsubmit(){
        try {
            
            let data = await apifetch("/user/login", {
                email: email, passw: passw
            })
            
         
            console.log(data)
        
            console.log("login success, session %s", data.sesskey) 
            saveSession(data.sesskey)
            window.location.href = "./home"
            

        } 
        catch (error) {
            err = ""+error;
            console.error(error)
        }
    }
    

</script>


<div class="flex flex-col h-screen justify-center items-center">

    <div class="w-[500px]  text-center p-4 bg-gray-50 rounded-3xl">

        <p class="text-3xl mb-3 text">Login</p>
        
        {#if err.length>0}
        <div class="text-sm text-red-600">
            {err}
        </div>
        {/if}

        <input bind:value={email} type="text" placeholder="Username" class="rounded-3xl mt-3 w-full">
        <input bind:value={passw} type="text" placeholder="Passwrod" class="rounded-3xl mt-3 w-full ">
        
        <div class="text-start mt-3 ml-2 flex justify-between">
            
            <!-- <div>
                <input bind:checked={rember} type="checkbox" id="c1" class="mb-1"/>
                <label for="c1" class="text-xl ml-1 mb-0.5">Remember me</label>
            </div>
            

            <div class="justify-self-end align">
                 <a href="http://google.com" class=" text-blue-500 hover:text-blue-700 text-xl cursor-pointer" >Register?</a>
            </div> -->
           
        </div>
        
        <div class="w-full text-start">
            <button on:click={onsubmit} class="bg-blue-500 hover:bg-blue-700 text-white rounded-3xl mt-3 ml-2 px-10 py-2 ">Submit</button>
        </div>
        
    
    </div>
</div>

