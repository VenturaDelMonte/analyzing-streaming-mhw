package uk.ac.imperial.lsds.saber.cql.expressions.doubles;

import uk.ac.imperial.lsds.saber.ITupleSchema;
import uk.ac.imperial.lsds.saber.buffers.IQueryBuffer;
import uk.ac.imperial.lsds.saber.cql.expressions.ExpressionsUtil;

public class DoubleColumnReference implements DoubleExpression {

	private int column;

	private final int size = 8;

	public DoubleColumnReference (int column) {

		if (column < 0)
			throw new IllegalArgumentException("error: column index must be greater than 0");

		this.column = column;
	}

	public int getColumn () {
		return column;
	}

	@Override
	public String toString() {
		final StringBuilder s = new StringBuilder();
		s.append("\"").append(column).append("\"");
		return s.toString();
	}

	public double eval (IQueryBuffer buffer, ITupleSchema schema, int offset) {

		return buffer.getDouble(offset + schema.getAttributeOffset(column));
	}

	public void appendByteResult (IQueryBuffer src, ITupleSchema schema, int offset, IQueryBuffer dst) {

		double value = eval(src, schema, offset);
        dst.putDouble(value);
	}

	public void writeByteResult (IQueryBuffer src, ITupleSchema schema, int srcOffset, IQueryBuffer dst, int dstOffset) {

		int pos = src.normalise(srcOffset + schema.getAttributeOffset(column));
		System.arraycopy(src.array(), pos, dst.array(), dstOffset, size);
	}

	public byte [] evalAsByteArray (IQueryBuffer buffer, ITupleSchema schema, int offset) {

		double value = eval(buffer, schema, offset);
		return ExpressionsUtil.doubleToByteArray(value);
	}

	public void evalAsByteArray (IQueryBuffer buffer, ITupleSchema schema, int offset, byte [] bytes) {

		double value = eval(buffer, schema, offset);
		ExpressionsUtil.doubleToByteArray(value, bytes);
	}

	public int evalAsByteArray(IQueryBuffer buffer, ITupleSchema schema, int offset, byte [] bytes, int pivot) {

		double value = eval(buffer, schema, offset);
		return ExpressionsUtil.doubleToByteArray(value, bytes, pivot);
	}
}
