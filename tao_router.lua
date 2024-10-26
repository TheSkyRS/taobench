-- Turn on the route library's verbose mode.
verbose(true)




pools {
    first_pool = {
        backend_options = { connecttimeout = 5},
        backends = {
            "127.0.0.1:11212",
            "127.0.0.1:11213",
        }
    },
    -- second_pool = {
    --     backend_options = { connecttimeout = 5},
    --     backends = {
    --         "127.0.0.1:11213",
    --     },
    -- },
}

routes {
    default = route_direct{
        child = "first_pool"
    },
}