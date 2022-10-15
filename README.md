# Narrative World Molds
Creates Narrative Worlds froms a knowledge base of plausable environments and the output of [Simplified Simulation](https://github.com/j-timothy-balint/Simplified-Simulation).
 
### Requirements
 1. Rapidjson (Link to the root of directory with the environmental variable RAPID_JSON_ROOT)
 2. sqlite3 (Link to root of directory with the environmental variable SQLITE_ROOT)
 
### Minimal Usage
Compile the program, and copy *graph_connections.txt* and *narrative_table.txt* into the root directory. When you run the main program, it will generate 100 total worlds (50 for the two provided methods) in a folder called *results* based on the data contained within *NarrativeWorldMold.db*.
 
### Citation
 If you find this work useful, include the following citation:
 
 ```
 @article{balint_procedural_appear,
  title={Procedural Generation of Narrative Worlds},
  author={Balint, J Timothy and Bidarra, Rafael},
  journal={IEEE Transactions on Games},
  year={to appear},
  publisher={IEEE}
}
 ```
