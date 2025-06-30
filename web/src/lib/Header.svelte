
<script>

    import icon from "$lib/icons/icon.png"
    import { saveSession } from '$lib';

    import {Navbar, NavBrand, NavHamburger, NavUl, NavLi} from 'flowbite-svelte';

    


    function onLogout(){
        saveSession("")
        setTimeout(()=>{
            window.location.href="./login"
        }, 500)
    }

</script>



<div class="">


    <Navbar fluid={true} let:hidden let:toggle>
        <NavBrand href="./">
            <img src={icon} class="me-3 h-6 sm:h-9" alt="ESPAdmin" />
            <span class="self-center whitespace-nowrap text-xl font-semibold dark:text-white">ESPAdmin</span>
        </NavBrand>
        <NavHamburger on:click={toggle} />
        <NavUl {hidden}>
            <NavLi href="./">Home</NavLi>
            <NavLi href="./#logout" on:click={onLogout}>Logout</NavLi>
        </NavUl>
    </Navbar>





</div>