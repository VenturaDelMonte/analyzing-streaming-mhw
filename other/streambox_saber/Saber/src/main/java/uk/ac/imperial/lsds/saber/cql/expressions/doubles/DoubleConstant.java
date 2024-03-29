package uk.ac.imperial.lsds.saber.cql.expressions.doubles;

import uk.ac.imperial.lsds.saber.ITupleSchema;
import uk.ac.imperial.lsds.saber.buffers.IQueryBuffer;
import uk.ac.imperial.lsds.saber.cql.expressions.ExpressionsUtil;

public class DoubleConstant implements DoubleExpression {

	private double constant;

	private byte [] b;

	public DoubleConstant(double constant) {

		this.constant = constant;

		b = ExpressionsUtil.doubleToByteArray(constant);
	}

	@Override
	public String toString() {
		final StringBuilder s = new StringBuilder();
		s.append("Constant ").append(constant);
		return s.toString();
	}

	public double eval (IQueryBuffer buffer, ITupleSchema schema, int offset) {
		return constant;
	}

	public void appendByteResult(IQueryBuffer src, ITupleSchema schema, int offset, IQueryBuffer dst) {

        dst.putDouble(constant);
	}

	public void writeByteResult(IQueryBuffer src, ITupleSchema schema, int srcOffset, IQueryBuffer dst, int dstOffset) {

		System.arraycopy(b, 0, dst.array(), dstOffset, b.length);
	}

	public byte [] evalAsByteArray (IQueryBuffer buffer, ITupleSchema schema, int offset) {

		return b;
	}

	public void evalAsByteArray (IQueryBuffer buffer, ITupleSchema schema, int offset, byte [] bytes) {

		evalAsByteArray(buffer, schema, offset, bytes, 0);
	}

	public int evalAsByteArray (IQueryBuffer buffer, ITupleSchema schema, int offset, byte [] bytes, int pivot) {

		System.arraycopy(b, 0, bytes, pivot, b.length);
		return (pivot + b.length);
	}
}
