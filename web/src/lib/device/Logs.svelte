
<script lang="ts">
    import { apifetch, type Device } from "$lib";
    import { Button, Checkbox, Label, Select } from "flowbite-svelte";

    const LOG_VERBOSE = 5
    const LOG_DEBUG = 4
    const LOG_INFO = 3
    const LOG_WARNING = 2
    const LOG_ERROR = 1
    const LOG_CRITICAL = 0


    type Log = {
            id: number
            timestamp: number
            level: number
            message: string
        }
    type propsType = {
        selectedDevice: Device | undefined
    }
    let {selectedDevice}: propsType = $props()
    let device = selectedDevice;

    let logs: Log[] = []
    let filterLogs: Log[] = $state([])

    let dates = getDates()
    let selectedDate = dates[0].value//$state(dates[0].value)
    // let date = selectedDate
    let fetching = 0
    let disabledLoadMore = $state(false)



    async function fetchData(){

        let path = `/device/${device?.devkey}/logs`
        // let path = `/device/e92211cbce531e417fadca9c381ffeaa/logs`

        try{
            let start = logs.length>0 ? logs[logs.length-1].id-1 : -1
            
            console.log(`fetching ${path} startid: ${start} date: ${selectedDate}`)
            fetching = 1
            let data = await apifetch(path, {
                startid: start,
                date: selectedDate
            })

            if(data.logs){
                if(data.logs.length > 0){
                    logs = [...logs, ...data.logs]
                    
                }
                else{
                    disabledLoadMore = true
                }

                sortAndFilter() 
            }
            else{
                console.error(data)
                throw Error("Invalid response data")
            }
            
        }catch(err){
            if(err instanceof Error)
                console.error(`fetch ${path} failed ${err.message}`)
        }
        finally{
            fetching = 0
        }
    }



    let verbose = $state(false)
    let debug = $state(true)
    let info = $state(true)
    let warn = $state(true)
    let error = $state(true)
    let critical = $state(true)

    function sortAndFilter(){
        logs.sort((a, b)=>{
            if (a.id < b.id) {
                return 1;
            }
            if (a.id > b.id) {
                return -1;
            }

            return 0;
        })

        filterLogs = logs.filter((log) => {
            return log.level == LOG_VERBOSE && verbose 
            || log.level == LOG_DEBUG && debug 
            || log.level == LOG_INFO && info
            || log.level == LOG_WARNING && warn
            || log.level == LOG_ERROR && error
            || log.level == LOG_CRITICAL && critical
        })

    }

    function getTime(timestamp: number){
        
        const date = new Date(timestamp*1000)
        // return date.toLocaleDateString() + " " + date.toLocaleTimeString()
        return date.toLocaleTimeString()
    }

    function getDate(timestamp: number){
    
        const date = new Date(timestamp*1000)
        // return date.toLocaleDateString() + " " + date.toLocaleTimeString()
        return date.toLocaleDateString()
    }

    function getDates()
    {
        let dates = []
        for (let i = 0; i < 7; i++) {
            let date = new Date(Date.now() - 3600*24*1000*i).toLocaleDateString()
            dates.push({value: date, name: date})
        }
        return dates
    }

    function getTextColor(log: Log){
        if(log.level==LOG_CRITICAL)
            return "text-red-700" //critical
        else if(log.level==LOG_ERROR)
            return "text-red-500" //error
        else if(log.level==LOG_WARNING)
            return "text-yellow-400" //warning
        else if(log.level==LOG_INFO)
            return "text-blue-500" //info
        else if(log.level==LOG_DEBUG)
            return "text-purple-400" //debug
        else if(log.level==LOG_VERBOSE)
            return "text-purple-200" //verbose
        else
            return "" 
    }

    function getTextStyle(log: Log){
        if(log.level==LOG_CRITICAL)
            return "font-bold" //critical
        else if(log.level==LOG_ERROR)
            return "font-semibold" //error
        else if(log.level==LOG_WARNING)
            return "font-semibold" //warning
        else if(log.level==LOG_INFO)
            return "font-semibold" //info
        else if(log.level==LOG_DEBUG)
            return "font-semibold" //debug
        else if(log.level==LOG_VERBOSE)
            return "font-semibold" //verbose
        else
            return "font-semibold" 
    }

    function onLoadMore(){
        if(!fetching)
            fetchData()
    }

    function onDateChanged()
    {
   

        // date = selectedDate
        disabledLoadMore = false
        logs = []
        if(!fetching)
            fetchData()

        
    }

    $effect(()=>{
        if(!fetching)
            fetchData()
    })


  



</script>
 



<div>


    <div class="w-full pb-3 min-h-[500px] h-[calc(100vh-200px)] ">

        <div class="flex gap-4 justify-between">
            <!-- <Label class="flex items-center">
                <Checkbox checked inline class="text-sky-400 focus:ring-pink-500" />
                Your custom color
            </Label> -->
            <div class="flex gap-4">
                
                <Checkbox bind:checked={verbose} color="purple" on:change={sortAndFilter} >Verbose</Checkbox>
                <Checkbox bind:checked={debug} color="purple" on:change={sortAndFilter} >Debug</Checkbox>
                <Checkbox bind:checked={info} color="blue" on:change={sortAndFilter}>Info</Checkbox>
                <Checkbox bind:checked={warn} color="yellow" on:change={sortAndFilter}>Warning</Checkbox>
                <Checkbox bind:checked={error} color="red" on:change={sortAndFilter}>Error</Checkbox> 
                <Checkbox bind:checked={critical} color="red" on:change={sortAndFilter} >Critical</Checkbox>
            </div>
            

            <Select class="w-[200px]" size="sm" placeholder="Choose a date" items={dates} bind:value={selectedDate} on:change={onDateChanged} />
            
          </div>


          <div class="w-full h-[87%] py-4 ">
                <div class="h-full p-2 border rounded-md border-blue-100 overflow-y-scroll">   

                            {#each filterLogs as log}
                                <div class="flex text-sm border-b-2 mt-3 justify-start items-center cursor-  " >
                                    <div class="w-[100px] text-center font-mono {getTextColor(log)} ">
                                        <span class="" >{getTime(log.timestamp)}</span>
                                        <span class="text-xs" >{getDate(log.timestamp)}</span>
                                    </div>
                                    
                                    <span class="w-full {getTextStyle(log)} {getTextColor(log)}">{log.message}</span>
                                </div>
                            {/each}
    
                </div>
          </div>

          <div class="w-full text-center">
            <Button class="" disabled={disabledLoadMore} color="light" on:click={onLoadMore}>Load more</Button>
          </div>
          


    </div>


</div>