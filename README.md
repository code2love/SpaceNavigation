# Value iteration

This code is an example for the value iteration algorithm. Additionally it uses MPI for distributed computing.
Data is loaded from the `data_debug` folder. When the computations are finished, the obtained optimal cost vector
`J` and the optimal strategy `Pi` are compared with a provided reference solution.

# Preparation

Make sure you can access all computers in your cluster via ssh without having to type in a username and password.
Do this by generating a public key via `ssh-keygen` and distribute it to the other machines via `ssh-copy-id`.

Also edit your `~/.ssh/config` file such that automatically the right user will be used, e.g:
```text
Host *
        User YourUserName
        ForwardX11 yes
        Compression yes
```

# Compile and Execute

Clone the repository and change into its folder. Create a `build` directory and change into it. There run `cmake .. && make`.
After successfull compilation you should be able to call `mpirun -np 8 -hostfile ../hostfile ./MPI_Project.exe`. 
Initially the hostfile only lists the localhost, change that according to your environment.



