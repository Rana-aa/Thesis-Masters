"""Decoding module."""
import numpy as np
import warnings
import test_pyldpc_utils as utils

from numba import njit, int64, types, float64

np.set_printoptions(linewidth=180, threshold=1000, edgeitems=20)

def decode(H, y, snr, maxiter=100):
    """Decode a Gaussian noise corrupted n bits message using BP algorithm.

    Decoding is performed in parallel if multiple codewords are passed in y.

    Parameters
    ----------
    H: array (n_equations, n_code). Decoding matrix H.
    y: array (n_code, n_messages) or (n_code,). Received message(s) in the
        codeword space.
    maxiter: int. Maximum number of iterations of the BP algorithm.

    Returns
    -------
    x: array (n_code,) or (n_code, n_messages) the solutions in the
        codeword space.

    """
    m, n = H.shape

    bits_hist, bits_values, nodes_hist, nodes_values = utils.bitsandnodes(H)

    var = 10 ** (-snr / 10)

    if y.ndim == 1:
        y = y[:, None]
    # step 0: initialization

    Lc = 2 * y / var
    _, n_messages = y.shape

    Lq = np.zeros(shape=(m, n, n_messages))

    Lr = np.zeros(shape=(m, n, n_messages))

    for n_iter in range(maxiter):
        #print(f'============================ iteration {n_iter} ============================')
        Lq, Lr, L_posteriori = _logbp_numba(bits_hist, bits_values, nodes_hist,
                                            nodes_values, Lc, Lq, Lr, n_iter)
        #print("Lq=", Lq.flatten())
        #print("Lr=", Lr.flatten())
        #print("L_posteriori=", L_posteriori.flatten())
        #print('L_posteriori=[')
        #for row in L_posteriori.reshape([-1, 16]):
        #    for val in row:
        #        cc = '\033[91m' if val < 0 else ('\033[92m' if val > 0 else '\033[94m')
        #        print(f"{cc}{val: 012.6g}\033[38;5;240m", end=', ')
        #    print()
        x = np.array(L_posteriori <= 0).astype(int)

        product = utils.incode(H, x)

        if product:
            print(f'found, n_iter={n_iter}')
            break

    if n_iter == maxiter - 1:
        warnings.warn("""Decoding stopped before convergence. You may want
                       to increase maxiter""")
    return x.squeeze()


output_type_log2 = types.Tuple((float64[:, :, :], float64[:, :, :],
                               float64[:, :]))


#@njit(output_type_log2(int64[:], int64[:], int64[:], int64[:], float64[:, :],
#                       float64[:, :, :],  float64[:, :, :], int64), cache=True)
def _logbp_numba(bits_hist, bits_values, nodes_hist, nodes_values, Lc, Lq, Lr,
                 n_iter):
    """Perform inner ext LogBP solver."""
    m, n, n_messages = Lr.shape
    # step 1 : Horizontal

    bits_counter = 0
    nodes_counter = 0
    for i in range(m):
        #print(f'=== i={i}')
        ff = bits_hist[i]
        ni = bits_values[bits_counter: bits_counter + ff]
        bits_counter += ff
        for j_iter, j in enumerate(ni):
            nij = ni[:]
            #print(f'\033[38;5;240mj={j:04d}', end=' ')

            X = np.ones(n_messages)
            if n_iter == 0:
                for kk in range(len(nij)):
                    if nij[kk] != j:
                        lcv = Lc[nij[kk],0]
                        lcc = '\033[91m' if lcv < 0 else ('\033[92m' if lcv > 0 else '\033[94m')
                        #print(f'nij={nij[kk]:04d} Lc={lcc}{lcv:> 8f}\033[38;5;240m', end=' ')
                        X *= np.tanh(0.5 * Lc[nij[kk]])
            else:
                for kk in range(len(nij)):
                    if nij[kk] != j:
                        X *= np.tanh(0.5 * Lq[i, nij[kk]])
            #print(f'\n==== {i:03d} {j_iter:01d} {X[0]:> 8f}')
            num = 1 + X
            denom = 1 - X
            for ll in range(n_messages):
                if num[ll] == 0:
                    Lr[i, j, ll] = -1
                elif denom[ll] == 0:
                    Lr[i, j, ll] = 1
                else:
                    Lr[i, j, ll] = np.log(num[ll] / denom[ll])
    # step 2 : Vertical

    for j in range(n):
        ff = nodes_hist[j]
        mj = nodes_values[bits_counter: nodes_counter + ff]
        nodes_counter += ff
        for i in mj:
            mji = mj[:]
            Lq[i, j] = Lc[j]

            for kk in range(len(mji)):
                if mji[kk] != i:
                    Lq[i, j] += Lr[mji[kk], j]

    # LLR a posteriori:
    L_posteriori = np.zeros((n, n_messages))
    nodes_counter = 0
    for j in range(n):
        ff = nodes_hist[j]
        mj = nodes_values[bits_counter: nodes_counter + ff]
        nodes_counter += ff
        L_posteriori[j] = Lc[j] + Lr[mj, j].sum(axis=0)

    return Lq, Lr, L_posteriori


def get_message(tG, x):
    """Compute the original `n_bits` message from a `n_code` codeword `x`.

    Parameters
    ----------
    tG: array (n_code, n_bits) coding matrix tG.
    x: array (n_code,) decoded codeword of length `n_code`.

    Returns
    -------
    message: array (n_bits,). Original binary message.

    """
    n, k = tG.shape

    rtG, rx = utils.gausselimination(tG, x)

    message = np.zeros(k).astype(int)

    message[k - 1] = rx[k - 1]
    for i in reversed(range(k - 1)):
        message[i] = rx[i]
        message[i] -= utils.binaryproduct(rtG[i, list(range(i+1, k))],
                                          message[list(range(i+1, k))])

    return abs(message)