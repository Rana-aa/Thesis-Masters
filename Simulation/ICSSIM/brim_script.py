# Import libraries and dependencies
import pandas as pd
import zed
import networkx as nx
import matplotlib.pyplot as plt
import ipaddress
import math
from collections import Counter
import networkx.algorithms.community as nxcom
import matplotlib
import numpy as np

# Zed Lake to query
lake = "ranaLake"

# Z Queries
zql = "from 'pool2' | count() by id.orig_h, id.resp_h, id.resp_p | sort id.orig_h, id.resp_h, id.resp_p"

# Create Zed client instance
client = zed.Client()

# Function to execute query and return DataFrame
def query_to_dataframe(lake, query):
    try:
        results = client.query(query)
        df = pd.json_normalize(results)
        return df
    except Exception as e:
        print(f"Failed to connect to Zed, check that Zed is running and lake {lake} exists: {e}")
        return pd.DataFrame()

# Function to check if an address is IPv4
def is_ipv4(address):
    try:
        return isinstance(ipaddress.ip_address(address), ipaddress.IPv4Address)
    except ValueError:
        return False

# Execute queries and create DataFrames
df = query_to_dataframe(lake, zql)

# Filter for only IPv4 addresses
if not df.empty:
    df = df[df["id.orig_h"].apply(is_ipv4) & df["id.resp_h"].apply(is_ipv4)]

# Check if DataFrame is created
if df.empty:
    print("Query did not return data or all data filtered out.")
else:
    print("Records in df: %s\n" % len(df))

    # Aggregate data to reduce complexity
    df_agg = df.groupby(['id.orig_h', 'id.resp_h', 'id.resp_p'], as_index=False)['count'].sum()
    df_agg.columns = ['source', 'target', 'port', 'conncount']

    # Verify aggregation
    print("Aggregated DataFrame:\n", df_agg.head(10))

    # Fill NaN fields with 0
    df_agg = df_agg.fillna(0)

    # Recast types
    df_agg["conncount"] = df_agg["conncount"].astype("int64")
    df_agg["port"] = df_agg["port"].astype("int64")

    # Calculate weights
    maxconncount = df_agg["conncount"].max()
    df_agg["connweight"] = df_agg.apply(lambda x: 10/maxconncount*x.conncount+0.1, axis=1)

    # Print min/max values for weights
    print(df_agg.connweight.min())
    print(df_agg.connweight.max())

    # Create graphs
    G = nx.from_pandas_edgelist(df_agg, source="source", target="target", edge_key="port", edge_attr=True, create_using=nx.MultiDiGraph())
    G2 = nx.from_pandas_edgelist(df_agg, source="source", target="target", edge_attr=True, create_using=nx.Graph())

    # Add node attributes
    src_attributes = ["connweight"]
    for index, row in df_agg.iterrows():
        src_attr_dict = {k: row.to_dict()[k] for k in src_attributes}
        G.nodes[row["source"]].update(src_attr_dict)
        G.nodes[row["target"]].update(src_attr_dict)

    # Determine graph size
    n = len(G.nodes)
    if n > 1000:
        largegraph = True
        fig_size = (50, 40)
        print("Large Graph: %s, using large graph functions" % n)
    elif 500 < n <= 999:
        fig_size = (40, 30)
        largegraph = False
        print("Small Graph: %s figsize: %s" % (n, fig_size))
    elif 250 < n <= 499:
        fig_size = (30, 20)
        largegraph = False
        print("Small Graph: %s figsize: %s" % (n, fig_size))
    else:
        fig_size = (18, 15)
        largegraph = False
        print("Small Graph: %s figsize: %s" % (n, fig_size))

    # Graph analysis
    print("Graph analysis\n")
    print("Multi-edge directed Graph\n")
    print("Number of nodes: %s\t" % len(G.nodes))
    print("Number of edges: %s\n" % len(G.edges))
    print("Graph density: %s\n" % nx.density(G))
    print("Graph is directed: %s\n" % G.is_directed())
    print("Graph is weighted: %s\n" % nx.is_weighted(G))
    print("Undirected Graph\n")
    print("Transivity: %s" % nx.transitivity(G2))
    print("Average clustering coefficient: %s" % nx.average_clustering(G2))
    communities = sorted(nxcom.greedy_modularity_communities(G2), key=len, reverse=True)
    print("Greedy Modularity Communities: %s" % len(communities))

    # Define colors for ports
    unique_ports = df_agg['port'].unique()
    cmap = matplotlib.colormaps.get_cmap('hsv')
    port_colors = {port: cmap(i / len(unique_ports)) for i, port in enumerate(unique_ports)}

    # Draw the graph with a single layout
    pos = nx.spring_layout(G)  # Use spring layout (force-directed)

    plt.figure(figsize=fig_size)
    for edge in G.edges(data=True):
        src, dst, attr = edge
        port = attr['port']
        color = port_colors[port]
        nx.draw_networkx_edges(G, pos, edgelist=[(src, dst)], width=0.5, style="solid", edge_color=[color], alpha=0.5, arrows=True, arrowsize=15, arrowstyle="-|>")

    nx.draw_networkx_nodes(G, pos, node_color="black", node_shape="D")
    nx.draw_networkx_labels(G, pos, font_color="red", font_size=18)
    plt.title("Force-directed")

    # Create a legend
    handles = [plt.Line2D([0], [0], color=port_colors[port], lw=2) for port in unique_ports]
    labels = [f'Port {port}' for port in unique_ports]
    plt.legend(handles, labels, title="Ports", loc='best')

    plt.gca().margins(0.20, 0.20)
    plt.tight_layout()

    # Save the figure
    plt.savefig('network_graph_ipv4_n2.png', format='png')

    # Show the figure
    plt.show(block=False)
